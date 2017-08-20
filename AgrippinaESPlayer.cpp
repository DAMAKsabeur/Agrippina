/*
 * (c) 1997-2016 Netflix, Inc.  All content herein is protected by
 * U.S. copyright and other applicable intellectual property laws and
 * may not be copied without the express permission of Netflix, Inc.,
 * which reserves all rights.  Reuse of any of this content for any
 * purpose without the permission of Netflix, Inc. is strictly
 * prohibited.
 */
#include "AgrippinaESPlayer.h"
#include "AgrippinaPlaybackGroup.h"
#include "AgrippinaESManager.h"
#include "AgrippinaSampleWriter.h"
#include <nrdbase/Log.h>
#include <nrdbase/ScopedMutex.h>
#include <nrdbase/MutexRanks.h>
#include <nrdbase/NFErr.h>
#include <nrdbase/Base64.h>
#include <nrd/NrdApplication.h>
#include <string.h>
#include <nrd/DrmManager.h>

using namespace netflix;
using namespace netflix::device;
using namespace netflix::device::esplayer;


AgrippinaESPlayer::AgrippinaESPlayer() : mCloseThreads(false),
                                   mEndOfStreamFlag(false),
                                   mDisabled(false),
                                   mRenderMode(false),
                                   mCurrentVolume(1.0),
                                   mDecoderTaskMutex(ESP_PLAYER_MUTEX, "ESPlayer Mutex")
{
}

AgrippinaESPlayer::~AgrippinaESPlayer()
{
    if(mDrmSession.get()){
        mDrmSession.reset();
        //NTRACE(TRACE_DRMSYSTEM, "[DrmSessionLifeCycle] AgrippinaESPlayer::%s drmSession.use_count %ld",
        //       __func__, mDrmSession.use_count());
    }
}

NFErr AgrippinaESPlayer::init(const struct StreamPlayerInitData& initData,
                           std::shared_ptr<IESPlayerCallback> callback,
                           AgrippinaPlaybackGroup* playbackGroup)
{
    NFErr err;

    NRDP_UNUSED(initData);

    mCallback = callback;
    assert(playbackGroup);
    mPlaybackGroup = playbackGroup;
    mDrmSession.reset();
    mPipelineId = mPlaybackGroup->getPipelineId();
    mPipelineIdString << mPipelineId;

    return err;
}

void AgrippinaESPlayer::close()
{
    mDrmSession.reset();
    //NTRACE(TRACE_DRMSYSTEM, "[DrmSessionLifeCycle] AgrippinaESPlayer::%s drmSession.use_count %ld",
    //       __func__, mDrmSession.use_count());
}

// This is called from the rendererTask thread. There's no mutex preventing the
// playback state from changing while this is being called. That's OK. The
// underflow event reaches the UI asynchronously anyway.
void AgrippinaESPlayer::underflowReporter()
{
    IPlaybackGroup::PlaybackState playbackState = mPlaybackGroup->getPlaybackState();

    if(playbackState == IPlaybackGroup::PLAY && inputsAreExhausted())
    {
        // If the end of stream flag is not raised, then this could be an
        // underflow.
        ScopedMutex cs(mDecoderTaskMutex);
        if(!mEndOfStreamFlag)
        {
            if(!mDisabled && !mCallback->reportUnderflowReceived())
            {
                mCallback->reportUnderflow();
            }
        } else
        {
            // If the mEndOfStreamFlag is raised, we send playbackCompleted(),
            // not underflow.
            if(!mCallback->playbackCompletedReceived()  &&
               playbackState == IPlaybackGroup::PLAY)
            {
                mCallback->playbackCompleted();
            }
        }
    }
}

NFErr AgrippinaESPlayer::decrypt(const ISampleAttributes *pSampleAttr,
                              AgrippinaSampleWriter& sampleWriter)
{
    uint8_t* data;
    uint8_t* bufferEnd; // points to the byte after the end of the buffer.
    uint32_t i;
    ullong offset = 0;
    NFErr err = NFErr_OK;

    if(pSampleAttr->getAlgorithmID() == 0)
    {
        /*
        if( pSampleAttr->getMediaAttributes() && pSampleAttr->getMediaAttributes()->mVideoAttributes != NULL)
            NTRACE(TRACE_DRMSYSTEM, "this is clear video stream");
        */
        return err;
    }

    netflix::device::IDrmSession::DataSegment segment;

    // on-the-fly license switching
    if(mDrmSession.get() && mDrmSession->getSessionState() == IDrmSession::InvalidState){
        // this means that, we should switch to different license
        //NTRACE(TRACE_DRMSYSTEM,
        //       "[DrmSessionLifeCycle] releasing sessionId %d licenseType %d for session switching",
        //       mDrmSession->getSessionId(), mDrmSession->getLicenseType());
        mDrmSession.reset();
    }

    // when there is no IDrmSession instance yet
    if(!mDrmSession.get()) {
        // get a new DrmSession
        std::string contentId = pSampleAttr->getContentId();
        NTRACE(TRACE_DRMSYSTEM, "stream's contentID(KeyID) %s\n", Base64::encode(contentId).c_str());

        mDrmSession
            = mPlaybackGroup->getESManager()->getMultiSessionDrmManager()->getDrmSession(contentId);

        if(mDrmSession.get()){
            //NTRACE(TRACE_DRMSYSTEM,
            //       "[DrmSessionLifeCycle] got session for player: sessionId %d, licenseType %d, use_count %ld",
            //       mDrmSession->getSessionId(), mDrmSession->getLicenseType(), mDrmSession.use_count());
            mContentId = contentId;
            err = mDrmSession->initDecryptContextByKid(contentId);
            if(!err.ok()){
                mContentId = "";
                return err;
            }
        } else {
            // 1. if start with bookmark where encrypted stream is, nrdjs is
            // supposed to send license before sending stream.
            //
            // 2. if start with non-enrypted stream, and reached encrypted
            // stream while buffering, we need to wait until license
            // arrives.
            return NFErr_Pending;
        }
    }

    // when there is an instance of IDrmSession, but contentId is
    // different. That is to say, content has been changed
    std::string contentId = pSampleAttr->getContentId();
    if(mContentId != contentId){
        // if content has been changed, we need to release current mDrmSession,
        // and get new one with new contentId
        NTRACE(TRACE_DRMSYSTEM, "stream's contentID(KeyID) changed %s\n", Base64::encode(contentId).c_str());

        // 1. release current IDrmSession instance
        //NTRACE(TRACE_DRMSYSTEM,
        //       "[DrmSessionLifeCycle] released drmSession for new content: sessionId %d, use_count %ld",
        //       mDrmSession->getSessionId(), mDrmSession.use_count());
        mDrmSession.reset();
            
        // 2. get new IDrmSession instance
        mDrmSession
            = mPlaybackGroup->getESManager()->getMultiSessionDrmManager()->getDrmSession(contentId);

        if(mDrmSession.get()){
            mContentId = contentId;
            err = mDrmSession->initDecryptContextByKid(contentId);
            if(!err.ok()){
                mContentId = "";
                return err;
            }
        } else {
            // if license for new content does not arrive in time, waiting
            // here
            NTRACE(TRACE_DRMSYSTEM, "Nrdlib have not received license yet for contentID(KeyID) %s", Base64::encode(contentId).c_str());
            return NFErr_Pending;
        }
    }

    bool isDrmSessionAlive = true;
    for(uint32_t viewNum = 0; viewNum < pSampleAttr->getNumViews(); viewNum++)
    {
        ScopedMutex systemLock(mDrmSession->getDecryptContextMutex());

        // on-the-fly license switching
        if(mDrmSession.get() && mDrmSession->getSessionState() == IDrmSession::InvalidState){
            // current mDrmSession might have been invalidated since we checked last time.
            isDrmSessionAlive = false;
            break;
        }

        // for debugging
        uint32_t viewSize = pSampleAttr->getSize(viewNum);
        uint32_t subSampleTotal = 0;

        data = sampleWriter.getBuffer(viewNum);

        // getSampleSize() is the length of the sample including any codec
        // specific data that has been prepended.
        bufferEnd = sampleWriter.getBuffer(viewNum) +
            sampleWriter.getSampleSize(viewNum);

        if(pSampleAttr->getSubsampleMapping(viewNum).size() == 0)
        {
            segment.data = data;
            segment.numBytes = pSampleAttr->getSize(viewNum);
            err = mDrmSession->decrypt(pSampleAttr->getIVData(),
                                       pSampleAttr->getIVSize(),
                                       offset, segment);
            offset += pSampleAttr->getSize(viewNum);
            subSampleTotal += pSampleAttr->getSize(viewNum);
        } else {
            for(i = 0; i< pSampleAttr->getSubsampleMapping(viewNum).size(); i+=2)
            {
                data += pSampleAttr->getSubsampleMapping(viewNum)[i];
                segment.data = data;
                segment.numBytes = pSampleAttr->getSubsampleMapping(viewNum)[i+1];

                subSampleTotal += pSampleAttr->getSubsampleMapping(viewNum)[i];
                subSampleTotal += pSampleAttr->getSubsampleMapping(viewNum)[i+1];

                if(segment.data + segment.numBytes > bufferEnd)
                {
                    Log::error(TRACE_MEDIAPLAYBACK, "Error in AgrippinaESPlayer::decrypt. "
                               " Subsample mapping is larger than the sample size. "
                               "Segment size: %u, segment end address %p,"
                               " bufferEndAddress %p", segment.numBytes,
                               segment.data + segment.numBytes, bufferEnd);

                    std::ostringstream ss;
                    ss << "Error in AgrippinaESPlayer::decrypt."
                       << "Subsample mapping is larger than the sample size.";
                    Variant errInfo;
                    errInfo["errorDescription"] = ss.str();
                    return err.push(new IDeviceError(DECRYPTION_ERROR,
                                                     AgrippinaAppDeviceSpecific_WrongSubsampleMapping,
                                                     errInfo));
                }
                /*
                printf("IVSize %d,\t, subSample [%d:%d]\n"
                       , pSampleAttr->getIVSize()
                       , pSampleAttr->getSubsampleMapping(viewNum)[i]
                       , pSampleAttr->getSubsampleMapping(viewNum)[i+1]);
                */
                err = mDrmSession->decrypt(pSampleAttr->getIVData(),
                                           pSampleAttr->getIVSize(),
                                           offset, segment);
                if(err != NFErr_OK)
                {
                    break;
                }
                data += pSampleAttr->getSubsampleMapping(viewNum)[i+1];
                offset += pSampleAttr->getSubsampleMapping(viewNum)[i+1];
            }
        }

        if( viewSize != subSampleTotal){
            Log::error(TRACE_MEDIAPLAYBACK,
                       "subSampleMapping : viewSize (%u) does not match subSampleTotal (%d)",
                       viewSize, subSampleTotal);
        }

        if(err != NFErr_OK) {
            break;
        }
    }

    if(isDrmSessionAlive == false)
    {
        mDrmSession.reset();
        //NTRACE(TRACE_DRMSYSTEM,
        //       "[DrmSessionLifeCycle] current drm session invalidated since we checked last time. released DrmSession for onTheFly DrmSession Switching use_count %ld",
        //      mDrmSession.use_count());
        return NFErr_Pending;
    }
    return err;
}


void AgrippinaESPlayer::updatePlaybackPosition(llong& currentPTS)
{
    mCallback->updatePlaybackPosition(currentPTS);
}

void AgrippinaESPlayer::disable()
{
    ScopedMutex cs(mDecoderTaskMutex);
    mDisabled = true;
}

void AgrippinaESPlayer::enable()
{
    ScopedMutex cs(mDecoderTaskMutex);
    mDisabled = false;
}

bool AgrippinaESPlayer::isDisabled()
{
    ScopedMutex cs(mDecoderTaskMutex);
    return mDisabled;
}

void AgrippinaESPlayer::setVideoPlaneProperties(VideoPlaneProperties)
{
    assert(0); // If this implementation is executing, this player is not a
               // video player.
}

VideoPlaneProperties AgrippinaESPlayer::getVideoPlaneProperties()
{
    assert(0); // If this implementation is executing, this player is not a
               // video player.
    return VideoPlaneProperties();
}

VideoPlaneProperties AgrippinaESPlayer::getPendingVideoPlaneProperties()
{
    assert(0); // If this implementation is executing, this player is not a
               // video player.
    return VideoPlaneProperties();
}

void AgrippinaESPlayer::commitVideoPlaneProperties()
{
    assert(0); // If this implementation is executing, this player is not a
               // video player.
}

bool AgrippinaESPlayer::closeThreadsFlagIsSet()
{
    ScopedMutex cs(mDecoderTaskMutex);
    return mCloseThreads;
}

void AgrippinaESPlayer::setCloseThreadsFlag()
{
   ScopedMutex cs(mDecoderTaskMutex);
   mCloseThreads = true;
}

void AgrippinaESPlayer::reportDecodeOutcome(DecoderStatsCounter& stats)
{
    // We update the stats every time we decode a frame. If there's more than
    // one error reported for the frame, we just report it as one frame that
    // produced decoding errors.
    int numFrameErrors = static_cast<int>(stats.getErrorCount() > 0);

    // We set the 3rd argument to STAT_NOT_SET because our decoders do not
    // produce low-quality decodes to maintain real-time performance.  If, for
    // example, the H.264 decoder could skip in-loop deblocking to save time, we
    // would specify in the 3rd argument the whether we did this or not.
    mCallback->updateDecodingStats(stats.getNumDecoded(),
                                   stats.getNumDropped(),
                                   IESPlayerCallback::STAT_NOT_SET,
                                   numFrameErrors);
    stats.resetCounters();


}

std::shared_ptr<IESPlayerCallback> AgrippinaESPlayer::getCallback()
{
    return mCallback;
}

void AgrippinaESPlayer::setVolume(double target, uint32_t duration, IAudioMixer::Ease)
{
    NRDP_UNUSED(duration);
    mCurrentVolume = target;
    NTRACE(TRACE_MEDIAPLAYBACK, "%s targetVolume:%f transitionDuration:%d", __func__, target, duration );
}

double AgrippinaESPlayer::getVolume()
{
    NTRACE(TRACE_MEDIAPLAYBACK, "%s %f", __func__, mCurrentVolume );
    return mCurrentVolume;
}

void AgrippinaESPlayer::setRenderMode(bool)
{
    assert(0); // If this implementation is executing, this player is not a video player.
}

bool AgrippinaESPlayer::getRenderMode()
{
    assert(0); // If this implementation is executing, this player is not a video player.
    return false;
}

bool AgrippinaESPlayer::updateTexture(void *, int * w, int * h, bool)
{
    assert(0); // If this implementation is executing, this player is not a video player.
    *w = 0;
    *h = 0;
    return false;
}
