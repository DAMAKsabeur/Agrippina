/*
 * (c) 1997-2016 Netflix, Inc.  All content herein is protected by
 * U.S. copyright and other applicable intellectual property laws and
 * may not be copied without the express permission of Netflix, Inc.,
 * which reserves all rights.  Reuse of any of this content for any
 * purpose without the permission of Netflix, Inc. is strictly
 * prohibited.
 */
#include <sstream>
#include "FileSystem.h"
#include "AgrippinaVideoESPlayer.h"
#include "AgrippinaESManager.h"
#include "AgrippinaPlaybackGroup.h"
#include "ESPlayerConstants.h"
#include "DeviceThread.h"
#include <nrdbase/Log.h>
#include <nrdbase/Time.h>
#include <nrdbase/Thread.h>
#include <nrd/AppThread.h>
#include <nrdbase/ScopedMutex.h>
#include <nrdbase/NFErr.h>
#include <nrd/NrdApplication.h>
#include "AgrippinaDeviceLib.h"

using namespace netflix;
using namespace netflix::device;
using namespace netflix::device::esplayer;

struct ivf_header {
    uint32_t dkif;
    uint16_t version;
    uint16_t header_size;
    uint32_t fourcc;
    uint16_t width;
    uint16_t height;
    uint32_t framerate;
    uint32_t timescale;
    uint32_t totalframes;
    uint32_t unused;
};

struct ivf_frame_header {
    uint32_t size;
    uint64_t pts;
};

Nero_video_codec_t AgrippinaVideoESPlayer::Nrd2NeroVideoCodec_Remap(const char* nrd_video_codec){
	Nero_video_codec_t NeroVideoCodec = NERO_CODEC_VIDEO_INVALID;
    Log::warn(TRACE_MEDIACONTROL,  ">>>> codec =%s  !!!\n",nrd_video_codec );
    if(strcmp (nrd_video_codec, "avc1") == 0)
    {
    	NeroVideoCodec = NERO_CODEC_VIDEO_H264;
    }
    else if(strcmp (nrd_video_codec, "mpg2") == 0)
    {
    	NeroVideoCodec = NERO_CODEC_VIDEO_MPEG2;
    }
    else if(strcmp (nrd_video_codec, "h264") == 0)
    {
    	NeroVideoCodec = NERO_CODEC_VIDEO_H264;
    }
    else if(strcmp (nrd_video_codec, "mpg4") == 0)
    {
    	NeroVideoCodec = NERO_CODEC_VIDEO_MPEG4;
    }
    else if(strcmp (nrd_video_codec, "jpeg") == 0)
    {
    	NeroVideoCodec = NERO_CODEC_VIDEO_JPEG;
    }
    return (NeroVideoCodec);
}

AgrippinaVideoESPlayer::~AgrippinaVideoESPlayer()
{
    setCloseThreadsFlag();
    (void)m_NeroVideoDecoder->UnInit();
    m_NeroVideoDecoder = NULL;
    m_stc = NULL;
}

static const Rect zeroRect = {0, 0, 0, 0};

AgrippinaVideoESPlayer::AgrippinaVideoESPlayer() : mCurrent3DFormat(NOT_3D),
                                             mCurrentVideoWindow(zeroRect),
                                             mCurrentAlpha(0.0),
                                             mCurrentZOrder(UINT_MAX),
                                             mCommitVideoWindow(zeroRect),
                                             mCommitAlpha(0.0),
                                             mCommitZOrder(UINT_MAX),
                                             mPendingVideoWindow(zeroRect),
                                             mPendingAlpha(0.0),
                                             mPendingZOrder(UINT_MAX),
                                             mTsOfLastFrameDeliveredToRenderer(INVALID_TIMESTAMP),
                                             mPendingSet(false),
                                             mFileDumping(false),
                                             mFileDumpingOnlyGop(false),
                                             mFp(NULL),
                                             mInputExhausted(false),
                                             mIsFlushing(false)
{
    if (sConfiguration->videoFileDump == "all") {
        mFileDumping = true;
    }

    if (sConfiguration->videoFileDump == "gop") {
        mFileDumping = true;
        mFileDumpingOnlyGop = true;
    }
}

AgrippinaVideoESPlayer::AgrippinaVideoESPlayer(NeroVideoDecoder* NeroVideoDecoder_ptr,NeroSTC* stc_ptr) : mCurrent3DFormat(NOT_3D),
                                             mCurrentVideoWindow(zeroRect),
                                             mCurrentAlpha(0.0),
                                             mCurrentZOrder(UINT_MAX),
                                             mCommitVideoWindow(zeroRect),
                                             mCommitAlpha(0.0),
                                             mCommitZOrder(UINT_MAX),
                                             mPendingVideoWindow(zeroRect),
                                             mPendingAlpha(0.0),
                                             mPendingZOrder(UINT_MAX),
                                             mTsOfLastFrameDeliveredToRenderer(INVALID_TIMESTAMP),
                                             mPendingSet(false),
                                             mFileDumping(false),
                                             mFileDumpingOnlyGop(false),
                                             mFp(NULL),
                                             mInputExhausted(false),
                                             mIsFlushing(false)
{
    if (sConfiguration->videoFileDump == "all") {
        mFileDumping = true;
    }

    if (sConfiguration->videoFileDump == "gop") {
        mFileDumping = true;
        mFileDumpingOnlyGop = true;
    }
    m_NeroVideoDecoder = NeroVideoDecoder_ptr;
    m_stc =stc_ptr;
}


void AgrippinaVideoESPlayer::close()
{
	Nero_error_t result = NERO_SUCCESS;
    setCloseThreadsFlag();
    // the manager releases the renderer.
    AgrippinaESPlayer::close();
    result = m_NeroVideoDecoder->Close();
    if (mFp) {
        fclose(mFp);
    }
}

NFErr AgrippinaVideoESPlayer::init(const struct StreamPlayerInitData& initData,
                                std::shared_ptr<IESPlayerCallback> callback,
                                AgrippinaPlaybackGroup *playbackGroup)

{
    NFErr err;
    Nero_error_t result = NERO_SUCCESS;
    Nero_video_codec_t NerovideoAlgo = NERO_CODEC_VIDEO_INVALID;
    AgrippinaESPlayer::init(initData, callback, playbackGroup);

    // Initialize the sample writer that we'll hand to the SDK to get access
    // units of encoded video. If this is MVC and we've requested MVC_SPLIT
    // initialize the sample writer to handle two views.
    mCurrent3DFormat = initData.mInitialStreamAttributes->mVideoAttributes->mFormat3D;
    mSampleWriter.reset(new AgrippinaSampleWriter(mCurrent3DFormat, callback));

    // Initialize decoder
    NerovideoAlgo = Nrd2NeroVideoCodec_Remap(initData.mInitialStreamAttributes->mVideoAttributes->mCodecParam.c_str());
    result = m_NeroVideoDecoder->Init(NerovideoAlgo);
    if(NERO_SUCCESS != result)
    {
        Log::error(TRACE_MEDIACONTROL,  ">>>> could not init Nero Video decoder for  =%s codec  !!!\n",initData.mInitialStreamAttributes->mVideoAttributes->mCodecParam.c_str());

    }
    // For debugging stream data
    if (mFileDumping) {
        mFilename = (Configuration::dataWritePath() + "/videodump");
        mFilename.append(1, '0'+mPipelineId);

        if (mVP9) {
            ivf_header ih;
            ih.dkif = _FOURCC_LE_('D','K','I','F');
            ih.version = 0;
            ih.header_size = sizeof(ih);
            ih.fourcc = _FOURCC_LE_('V','P','9','0');
            ih.width = initData.mMaxWidth;
            ih.height = initData.mMaxHeight;
            ih.framerate = 1000;
            ih.timescale = 1;
            ih.totalframes = 0;
            ih.unused = 0;
            mFilename.append(".ivf");
            mFp = fopen(mFilename.c_str(), "wb");
            NRDP_CHECK(fwrite(&ih, sizeof(ivf_header), 1, mFp), 1);
        } else {
            mFilename.append(".es");
            mFp = fopen(mFilename.c_str(), "wb");
        }
    }

    // Start the decoder and render threads.
    mVideoDecoderThread.reset(
        new DeviceThread(*this, &AgrippinaESPlayer::DecoderTask, &THREAD_REFERENCE_DPI_VIDEO_DECODER));
    mVideoEventThread.reset(
        new DeviceThread(*this, &AgrippinaESPlayer::EventTask, &THREAD_REFERENCE_DPI_VIDEO_EVENT));
    return err;
}
void AgrippinaVideoESPlayer::EventHandling(NeroEvents_t *event)
{
    switch (event->header)
    {
         case     NERO_EVENT_FRAME_FLIPPED :
        {
    	    NTRACE(TRACE_MEDIAPLAYBACK ,"NERO_EVENT_FRAME_FLIPPED ...");
    	    mCallback->updatePlaybackPosition(event->pts);
        break;
        }
        case     NERO_EVENT_UNDERFLOW     :
        {
    	    NTRACE(TRACE_MEDIAPLAYBACK ,"NERO_EVENT_UNDERFLOW ...");
    	    break;
        }
        case     NERO_EVENT_UNDERFLOWRECOVRED      :
        {
    	    NTRACE(TRACE_MEDIAPLAYBACK ,"NERO_EVENT_OVERFLOW ...");
    	    break;
        }
        case     NERO_EVENT_LAST          :
        default:
        {
            NTRACE(TRACE_MEDIAPLAYBACK ,"NERO_EVENT_LAST ERROR");
    	    break;
        }
    }
}

void AgrippinaVideoESPlayer::EventTask()
{
    const Time EVENT_TASK_WAIT_TIME(100);
    NeroEvents_t event;

    while(closeThreadsFlagIsSet() == false)
    {
        if (m_NeroVideoDecoder->NeroEventWait(&event) == NERO_SUCCESS)
        {
        	EventHandling(&event);
        }
        Thread::sleep(EVENT_TASK_WAIT_TIME);
    }
}

void AgrippinaVideoESPlayer::DecoderTask()
{
    static const Time WAIT_FOR_VIDEO_FRAME_BUFFER(10);
    static const Time WAIT_FOR_VIDEO_DATA(100);
    static const Time WAIT_WHILE_IDLING(100);

    const ISampleAttributes *pSampleAttr = NULL;
    bool errorReported = false;
    NFErr err = NFErr_OK;

    while(closeThreadsFlagIsSet() == false)
    {

        if(mIsFlushing)
        {
            {
                ScopedMutex cs(mDecoderTaskMutex);
                mInputExhausted = true;
                mEndOfStreamFlag = false;
            }
            Thread::sleep(ESPlayerConstants::WAIT_FOR_FLUSH_OFF);
            continue;
        }

        if(errorReported)
        {
            Thread::sleep(WAIT_WHILE_IDLING);
            continue;
        }

        if(!mEndOfStreamFlag)
        {
            Status status;

            // Check if any access-unit data is available to decode.
            status = mCallback->getNextMediaSample(*mSampleWriter);
            if(status == NO_AVAILABLE_SAMPLES) {
                pSampleAttr = NULL;
                mInputExhausted = true;
                NTRACE(TRACE_MEDIAPLAYBACK
                       ,"[%d] AgrippinaVideoESPlayer::decoderTask::getNextMediaSample returns NO_AVAILABLE_SAMPLES"
                       ,mPipelineId);
                Thread::sleep(WAIT_FOR_VIDEO_DATA);
                continue;
            } else if(status == NO_AVAILABLE_BUFFERS) {
                pSampleAttr = NULL;
                NTRACE(TRACE_MEDIAPLAYBACK
                       ,"[%d] AgrippinaVideoESPlayer::decoderTask::getNextMediaSample returns NO_AVAILABLE_BUFFERS"
                       ,mPipelineId);
                Thread::sleep(WAIT_FOR_VIDEO_DATA);
                continue;
            } else if (status == END_OF_STREAM) {
                {
                    ScopedMutex cs(mDecoderTaskMutex);
                    mEndOfStreamFlag = true;
                   // mVideoDecoder->setEndOfStream(true);
                }
                mInputExhausted = true;
                mSampleWriter->reset();
                pSampleAttr = NULL; // As a precaution.  GetNextMediaSample
                // should set it to NULL.
                NTRACE(TRACE_MEDIAPLAYBACK
                       ,"[%d] AgrippinaVideoESPlayer::decoderTask::getNextMediaSample returns END_OF_STREAM"
                       ,mPipelineId);
            } else if(status == STREAM_ERROR) {
                Log::error(TRACE_MEDIAPLAYBACK
                           ,"[%d] AgrippinaVideoESPlayer::decoderTask::getNextMediaSample returns STREAM_ERROR."
                           ,mPipelineId);
                errorReported = true;
                continue;
            } else if (status == ERROR) {
                Log::error(TRACE_MEDIAPLAYBACK,
                           "[%d] AgrippinaVideoESPlayer::decoderTask::getNextMediaSample returns ERROR. "
                           ,mPipelineId);
                errorReported = true;
                continue;
            } else {
                mInputExhausted = true;
                pSampleAttr = mSampleWriter->getSampleAttributes();

                // loop around decrypt until we get drmsession object for
                // current sample. Once we retrieve sample, we should keep it
                // until we consume it.
                while(mIsFlushing == false){
                    err = decrypt(pSampleAttr, *mSampleWriter);
                    if(!err.ok()) {
                        // with multisession drm, even if contract between JS and
                        // nrdlib/app mandate DRMed content is delivered only after
                        // licence is installed, just for safety, we wait in a loop
                        // until license is arrived and corresponding drm session is
                        // created.
                        if (err == NFErr_Pending) {
                            Thread::sleep(WAIT_FOR_VIDEO_DATA);
                            continue;
                        }

                        Log::error(TRACE_MEDIAPLAYBACK
                                   ,"[%d] AgrippinaVideoESPlayer::decoderTask: error in decrypt."
                                   ,mPipelineId);
                        mCallback->reportError(err);
                        errorReported = true;
                    }
                    break;
                }

                if (mIsFlushing) {
                    continue;
                }

                if(errorReported){
                    continue;
                }
            }
        }

        // Lock a frame-buffer, decode the access-unit and write the decoded
        // picture into the frame-buffer. The decoder may or may not output a
        // picture.
        {
            if(pSampleAttr){
                NTRACE(TRACE_MEDIAPLAYBACK
                       , "[%d] AgrippinaVideoESPlayer::%s decode pts %lld, dts %lld"
                       , mPipelineId, __func__, pSampleAttr->getPTS(), pSampleAttr->getDTS());
            }

            if(mIsFlushing || closeThreadsFlagIsSet()){
                NTRACE(TRACE_MEDIAPLAYBACK
                       , "[%d] AgrippinaVideoESPlayer::%s flush was issued. Abort decoding pts %lld, dts %lld"
                       , mPipelineId, __func__, pSampleAttr->getPTS(), pSampleAttr->getDTS());
                continue;
            }

            const unsigned char* auData = mSampleWriter->getBuffer();
            const uint32_t auDataSize = mSampleWriter->getSampleSize();

            if (pSampleAttr && auData && auDataSize) {
                if (mFileDumpingOnlyGop) {
                    if (pSampleAttr->isKeyFrame()) {
                        off_t pos = 0;
                        if (mVP9) {
                            pos = sizeof(ivf_header);
                        }
                        NRDP_CHECK(fseek(mFp, pos, SEEK_SET), 0);
                        NRDP_CHECK(ftruncate(fileno(mFp), pos), 0);
                        NRDP_CHECK(fflush(mFp), 0);
                    }
                }

                if (mFileDumping) {
                    if (mVP9) {
                        ivf_frame_header fh;
                        fh.size = auDataSize;
                        fh.pts = pSampleAttr->getPTS();
                        NRDP_CHECK(fwrite(&fh, sizeof(ivf_frame_header), 1, mFp), 1);
                    }
                    NRDP_CHECK(fwrite(auData, sizeof(char), auDataSize, mFp), (size_t)auDataSize);
                }

              // Partner should feed actual data (mSampleWriter->getBuffer())
              // into the decoder here
              m_NeroVideoDecoder->Feed(mSampleWriter->getBuffer(), mSampleWriter->getSampleSize(),(uint64_t)pSampleAttr->getPTS(),false);
              //mVideoDecoder->feedDecoder(pSampleAttr->getPTS());
            }

            // Report that a frame was decoded or skipped and report whether there were any non-fatal errors.
            //reportDecodeOutcome(mVideoDecoder->getStatsCounter());

        }
    }
}


// If the mCurrentFormat3D is MVC_SPLIT, logs that.  Otherwise it logs
// the type entered as the argument.
void AgrippinaVideoESPlayer::log3DType(Format3D format3D)
{
    std::string format;

    if(mCurrent3DFormat == MVC_SPLIT)
    {
        format = "MVC_SPLIT";
    } else if(format3D == MVC_COMBINED){
        format = "MVC_COMBINED";
    } else {
        return;
        // Don't log  NOT_3D
    }

    NTRACE(TRACE_MEDIAPLAYBACK
           ,"[%d] AgrippinaVideoESPlayer:decoderTask: Elementary stream's 3D format is %s"
           ,mPipelineId, format.c_str());
}

bool AgrippinaVideoESPlayer::readyForPlaybackStart()
{
    {
        ScopedMutex cs(mDecoderTaskMutex);
        if(mEndOfStreamFlag == true)
        {
            return true;
        }
    }

    // PARTNER: The reference application uses the current renderer state to
    // determine if we should return true here.  Partners will need to set
    // a flag in the decoder context when their decoder buffer has enough
    // data.  In this current state, we will never go ready!
    return true;
}

bool AgrippinaVideoESPlayer::inputsAreExhausted()
{
    return mInputExhausted;
}

void AgrippinaVideoESPlayer::flush()
{
    // Not implemented.  Video is only flushed via a flush on the entire
    // playback group.

    (void)m_NeroVideoDecoder->Flush();
}

void AgrippinaVideoESPlayer::beginFlush()
{
	(void)m_NeroVideoDecoder->Flush();
    mIsFlushing = true;
}


void AgrippinaVideoESPlayer::endFlush()
{
    mIsFlushing = false;
    {
        ScopedMutex cs(mDecoderTaskMutex);
        mEndOfStreamFlag = false;
    }
    //mVideoDecoder->setEndOfStream(false);
}

void AgrippinaVideoESPlayer::setVideoPlaneProperties(VideoPlaneProperties p)
{
	Nero_error_t result = NERO_SUCCESS;
    NTRACE(TRACE_MEDIAPLAYBACK,
           "[%d] AgrippinaVideoESPlayer::setVideoPlaneProperties %d %d %d %d (z=%d alpha=%f)",
           mPipelineId,
           p.rect.x, p.rect.y, p.rect.width, p.rect.height,
           p.zorder,
           static_cast<double>(p.alpha)
        );

    mPendingVideoWindow = p.rect;
    mPendingAlpha = p.alpha;
    mPendingZOrder = p.zorder;
    NTRACE(TRACE_MEDIAPLAYBACK,">>>>>>>>> Set video Plane properties");
    result = m_NeroVideoDecoder->ResizeLayer(p.rect.width , p.rect.height ,p.rect.x,p.rect.y);
    mPendingSet = true;
}

VideoPlaneProperties AgrippinaVideoESPlayer::getVideoPlaneProperties()
{
    NTRACE(TRACE_MEDIAPLAYBACK,
           "[%d] AgrippinaVideoESPlayer::getVideoPlaneProperties %d %d %d %d (z=%d alpha=%f)",
           mPipelineId,
           mCurrentVideoWindow.x, mCurrentVideoWindow.y, mCurrentVideoWindow.width, mCurrentVideoWindow.height,
           mCurrentZOrder,
           mCurrentAlpha
        );

    VideoPlaneProperties t;
    t.rect      = mCurrentVideoWindow;
    t.alpha     = mCurrentAlpha;
    t.zorder    = mCurrentZOrder;
    return t;
}

VideoPlaneProperties AgrippinaVideoESPlayer::getPendingVideoPlaneProperties()
{
    NTRACE(TRACE_MEDIAPLAYBACK,
           "[%d] AgrippinaVideoESPlayer::getPendingVideoPlaneProperties %d %d %d %d (z=%d alpha=%f)",
           mPipelineId,
           mPendingVideoWindow.x, mPendingVideoWindow.y, mPendingVideoWindow.width, mPendingVideoWindow.height,
           mPendingZOrder,
           mPendingAlpha
        );

    VideoPlaneProperties t;
    t.rect      = mPendingVideoWindow;
    t.alpha     = mPendingAlpha;
    t.zorder    = mPendingZOrder;
    return t;
}

void AgrippinaVideoESPlayer::commitVideoPlaneProperties()
{
    NTRACE(TRACE_MEDIAPLAYBACK,
           "[%d] AgrippinaVideoESPlayer::commitVideoPlaneProperties %d %d %d %d (z=%d alpha=%f)",
           mPipelineId,
           mPendingVideoWindow.x, mPendingVideoWindow.y, mPendingVideoWindow.width, mPendingVideoWindow.height,
           mPendingZOrder,
           static_cast<double>(mPendingAlpha)
        );

    if (mPendingSet) {
        mCommitVideoWindow = mPendingVideoWindow;
        mCommitAlpha       = mPendingAlpha;
        mCommitZOrder      = mPendingZOrder;
        mPendingSet = false;
    }
}

void AgrippinaVideoESPlayer::updateTransition()
{
    // Only called in renderer task
    if ( (mCommitVideoWindow.x != mCurrentVideoWindow.x) ||
         (mCommitVideoWindow.y != mCurrentVideoWindow.y) ||
         (mCommitVideoWindow.width != mCurrentVideoWindow.width) ||
         (mCommitVideoWindow.height != mCurrentVideoWindow.height) ||
         (mCommitAlpha != mCurrentAlpha) ||
         (mCommitZOrder != mCurrentZOrder))
    {
        // Partner should set video window here

        mCurrentVideoWindow = mCommitVideoWindow;
        mCurrentAlpha = mCommitAlpha;
        mCurrentZOrder = mCommitZOrder;
    }
}

MediaType AgrippinaVideoESPlayer::getMediaType()
{
    return VIDEO;
}

void AgrippinaVideoESPlayer::setRenderMode(bool textureMode)
{
    NTRACE(TRACE_MEDIAPLAYBACK, "[%d] AgrippinaVideoESPlayer::setRenderMode textureMode=%d (%s)", mPipelineId, textureMode, textureMode?"texture":"plane");
    // Partner should set the render mode here

    mRenderMode = textureMode;
}

bool AgrippinaVideoESPlayer::getRenderMode()
{
    return mRenderMode;
}

bool AgrippinaVideoESPlayer::updateTexture(void * v, int * width, int * height, bool forceUpdate)
{
    NRDP_UNUSED(v);
    NRDP_UNUSED(forceUpdate);

    // Partner should update texture here

    if (width) *width = 0;
    if (height) *height = 0;
    return false;
}
