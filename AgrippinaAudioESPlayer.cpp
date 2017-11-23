/*
 * (c) 1997-2016 Netflix, Inc.  All content herein is protected by
 * U.S. copyright and other applicable intellectual property laws and
 * may not be copied without the express permission of Netflix, Inc.,
 * which reserves all rights.  Reuse of any of this content for any
 * purpose without the permission of Netflix, Inc. is strictly
 * prohibited.
 */
#include "AgrippinaAudioESPlayer.h"
#include "AgrippinaPlaybackGroup.h"
#include "AgrippinaESManager.h"
#include "ESPlayerConstants.h"
#include "DeviceThread.h"
#include "AgrippinaSampleWriter.h"
#include <nrd/AppThread.h>
#include <nrdbase/Log.h>
#include <nrdbase/Time.h>
#include <nrdbase/Thread.h>
#include <nrdbase/ScopedMutex.h>
#include <nrdbase/NFErr.h>

using namespace netflix::device::esplayer;
using namespace netflix;

Nero_audio_codec_t AgrippinaAudioESPlayer::Nrd2NeroAudioCodec_Remap(const char* nrd_audio_codec)
{
	Nero_audio_codec_t NeroAudioCodec = NERO_CODEC_AUDIO_INVALID;
    Log::error(TRACE_MEDIACONTROL,  ">>>> codec =%s  !!!\n",nrd_audio_codec );
    if(strcmp (nrd_audio_codec, "ec-3") == 0)
    {
    	NeroAudioCodec = NERO_CODEC_AUDIO_DD_PLUS;
    }
    else if(strcmp (nrd_audio_codec, "mp4a") == 0)
    {
    	NeroAudioCodec = NERO_CODEC_AUDIO_AAC;
    }

    return(NeroAudioCodec);
}


AgrippinaAudioESPlayer::~AgrippinaAudioESPlayer()
{
	Nero_error_t result = NERO_SUCCESS;
    setCloseThreadsFlag();
    result = m_NeroAudioDecoder->UnInit();
	m_NeroAudioDecoder = NULL;
	m_stc = NULL;
}

void AgrippinaAudioESPlayer::close()
{
	Nero_error_t result = NERO_SUCCESS;
    setCloseThreadsFlag();
    AgrippinaESPlayer::close();
    result = m_NeroAudioDecoder->Close();
}

AgrippinaAudioESPlayer::AgrippinaAudioESPlayer() :
  mPlaybackPending(false),
  mInputExhausted(false),
  mIsFlushing(false)
{

}

AgrippinaAudioESPlayer::AgrippinaAudioESPlayer(NeroAudioDecoder* NeroAudioDecoder_ptr,NeroSTC* stc_ptr) :
  mPlaybackPending(false),
  mInputExhausted(false),
  mIsFlushing(false)
{
	m_NeroAudioDecoder = NeroAudioDecoder_ptr;
	m_stc = stc_ptr;

}

NFErr AgrippinaAudioESPlayer::init(const struct StreamPlayerInitData& initData,
                                std::shared_ptr<IESPlayerCallback> callback,
								AgrippinaPlaybackGroup* playbackGroup)
{
    NFErr err;
    Nero_error_t result = NERO_SUCCESS;
    Nero_audio_codec_t NeroAudioAlgo = NERO_CODEC_AUDIO_INVALID;

    AgrippinaESPlayer::init(initData, callback, playbackGroup);
    uint32_t samplingRate = 48000;
    NRDP_UNUSED(samplingRate);

    if(initData.mInitialStreamAttributes->mAudioAttributes->mSamplesPerSecond != 0)
    {
        samplingRate = initData.mInitialStreamAttributes->
            mAudioAttributes->mSamplesPerSecond;

    }
    NeroAudioAlgo = Nrd2NeroAudioCodec_Remap(initData.mInitialStreamAttributes->mAudioAttributes->mCodecParam.c_str());
    NeroAudioFMT = NeroAudioAlgo;
    result = m_NeroAudioDecoder->Init((size_t)NeroAudioAlgo);
    if(NERO_SUCCESS != result)
    {
        Log::error(TRACE_MEDIACONTROL,  ">>>> could not init Nero Audio decoder for  =%s codec  !!!\n",initData.mInitialStreamAttributes->mAudioAttributes->mCodecParam.c_str());

    }
    mSampleWriter.reset(new AgrippinaSampleWriter(NOT_3D, callback));

    mAudioDecoderThread.reset(new DeviceThread(*this, &AgrippinaESPlayer::DecoderTask,
                                               &THREAD_REFERENCE_DPI_AUDIO_DECODER));
    mAudioEventThread.reset(new DeviceThread(*this, &AgrippinaESPlayer::EventTask,
                                               &THREAD_REFERENCE_DPI_AUDIO_EVENT));

    return err;
}

void AgrippinaAudioESPlayer::EventHandling(NeroEvents_t *event)
{


}
void AgrippinaAudioESPlayer::EventTask()
{
    const Time EVENT_TASK_WAIT_TIME(100);
    NeroEvents_t event;

    while(closeThreadsFlagIsSet() == false)
    {
        if (m_NeroAudioDecoder->NeroEventWait(&event) == NERO_SUCCESS)
        {
        	EventHandling(&event);
        }
        Thread::sleep(EVENT_TASK_WAIT_TIME);
    }
}
void AgrippinaAudioESPlayer::DecoderTask()
{
    static const Time WAIT_FOR_AUDIO_SAMPLE_BUFFER(30);
    static const Time WAIT_FOR_AUDIO_DATA(100);
    static const Time WAIT_WHILE_IDLING(100);
    Status status;
    const ISampleAttributes* pSampleAttr;
    DecodedAudioBuffer decodedAudioBuffer;
    bool errorReported = false;
    NFErr err = NFErr_OK;
    bool discontinuity =false;
    Nero_error_t result = NERO_SUCCESS;
    Nero_audio_codec_t NeroAudioAlgo = NERO_CODEC_AUDIO_INVALID;
    while(closeThreadsFlagIsSet() == false)
    {
        if(mIsFlushing)
        {
            {
                ScopedMutex cs(mDecoderTaskMutex);
                mIsFlushing = true;
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

        // Playback may be pending if the player was enabled while the playback
        // group's playback state was PLAY. This would be the case after an
        // on-the-fly audio switch, for example. When playback is pending, the
        // renderer continues to act as if the player is disabled (e.g. it
        // doesn't render output and it doesn't report underflow). If we have
        // enough audio frames buffered to begin playback, we should lower the
        // pending flag and let playback begin.
        if(playbackPending() && readyForPlaybackStart())
        {
            setPlaybackPending(false);
        }

        status = mCallback->getNextMediaSample(*mSampleWriter);
        if(status == NO_AVAILABLE_SAMPLES) {
            mInputExhausted = true;
            Thread::sleep(WAIT_FOR_AUDIO_DATA);
            continue;
        } else if (status == NO_AVAILABLE_BUFFERS)
        {
            Thread::sleep(WAIT_FOR_AUDIO_DATA);
            continue;
        } else if (status == END_OF_STREAM)
        {
            // This is the only thread that sets mEndOfStream.  We don't need
            // the mutex to read it.
            if(mEndOfStreamFlag == false)
            {
                NTRACE(TRACE_MEDIAPLAYBACK, "AgrippinaAudioESPlayer::decoderTask: "
                       "getNextMediaSample returns END_OF_STREAM");
            }
            {
                // The renderer thread reads mEndOfStreamFlag. We need the mutex
                // to set it.
                ScopedMutex cs(mDecoderTaskMutex);
                mEndOfStreamFlag = true;
            }
            mInputExhausted = true;
            Thread::sleep(WAIT_FOR_AUDIO_DATA);
            continue;
        } else if (status == STREAM_ERROR) {
            Log::error(TRACE_MEDIAPLAYBACK, "AgrippinaAudioESPlayer::decoderTask: "
                       "getNextMediaSample returns STREAM_ERROR. ");
            errorReported = true;
            continue;
        } else if (status == ERROR) {
            Log::error(TRACE_MEDIAPLAYBACK, "AgrippinaAudioESPlayer::decoderTask: "
                       "getNextMediaSample returns ERROR. ");
            errorReported = true;
            continue;
        } else {
            pSampleAttr = mSampleWriter->getSampleAttributes();

            /*
             * audio gap handling
             */
            if ( pSampleAttr->getDiscontinuityGapInMs() > 1 ){
                // this means that there is a discontinuity in fqront of this sample.
                // send silent data corresponding to gap amount to decodedAudioBuffer
                NTRACE(TRACE_MEDIAPLAYBACK, "AgrippinaAudioESPlayer::decoderTask: audioPtsGap %lld",
                       pSampleAttr->getDiscontinuityGapInMs());
                discontinuity = true;
            }

            err = decrypt(pSampleAttr, *mSampleWriter);
            if(!err.ok())
            {
                NTRACE(TRACE_MEDIAPLAYBACK, "AudioESPlayer received encrypted data. Audio stream should be clear.");
                mCallback->reportError(err);
                errorReported = true;
                continue;
            }
        }
        mInputExhausted = false;

        if(pSampleAttr->getMediaAttributes() != 0)
        {
            // Update the audio decoder attributes after each discontinuity
            // (right after open() or flush()). This is because the selected
            // audio stream might be different after discontinuity, we must
            // update the corresponding audio attributes to the decoder.
            assert(pSampleAttr->getMediaAttributes()->mAudioAttributes);
            NTRACE(TRACE_MEDIAPLAYBACK, "AudioESPlayer::decoderTask: updating audio decoder attributes"
                   " because sample media attributes are non-null.");

            if(pSampleAttr->getMediaAttributes()->mAudioAttributes != NULL)
            {
            	Log::error(TRACE_MEDIAPLAYBACK, "hammadiiiiiiiii");
                // Reset audio device based on the audio sampling frequency of
                // the selected audio stream.
                const uint32_t samplesPerSecond = pSampleAttr->getMediaAttributes()->mAudioAttributes->mSamplesPerSecond;
                NRDP_UNUSED(samplesPerSecond);
                // Partner should check the current audio settings, if they have
                // changed, then they need to reconfigure their decoder.
                NeroAudioAlgo = Nrd2NeroAudioCodec_Remap(pSampleAttr->getMediaAttributes()->mAudioAttributes->mCodecParam.c_str());
                if (NeroAudioAlgo != NeroAudioFMT){
                    result = m_NeroAudioDecoder->Reconfigure((size_t)NeroAudioAlgo);
                    NeroAudioFMT = NeroAudioAlgo;
                    Log::error(TRACE_MEDIAPLAYBACK, "NeroAudioAlgo %d",NeroAudioFMT);
                }
            }
        }

        if(true /* Feed the decoder true */ )
        {
            // push buffers down to audio renderer here.
            // sampleWriter.getBuffer() pointer to the buffer
            // sampleWriter.getSampleSize() size of the sample
        	NTRACE(TRACE_MEDIAPLAYBACK, "Feed Audio DATA");
        	m_NeroAudioDecoder->Feed(mSampleWriter->getBuffer(), mSampleWriter->getSampleSize(),(uint64_t)pSampleAttr->getPTS(),discontinuity);
        }
        else
        {
            Log::error(TRACE_MEDIAPLAYBACK, "Error AgrippinaAudioESPlayer::decoderTask: decode returns false.");

            /* We don't call mCallback->reportError() here so that an issue
             * decoding one audio frame doesn't force the UI to unload the
             * movie.  We continue decoding and try recover on the next
             * sample.*/

            if(mIsFlushing){
                NTRACE(TRACE_MEDIAPLAYBACK, "[%d] AgrippinaAudioESPlayer::%s ignoring decode error while flushing"
                       , mPipelineId, __func__);
                continue;
            }

            if(closeThreadsFlagIsSet() == true){
                NTRACE(TRACE_MEDIAPLAYBACK, "[%d] AgrippinaAudioESPlayer::%s ignoring decode error while closing"
                       , mPipelineId, __func__);
                return;
            }
        }
        // if possible, partner should report dropped frames through this method
        // reportDecodeOutcome(mAudioDecoder->getStatsCounter());
    }
}

bool AgrippinaAudioESPlayer::readyForPlaybackStart()
{
    {
        // The decoder thread sets mEndOfStreamFlag. This method is called on
        // the SDK's pumping loop thread. We need the mutex to read it.
        ScopedMutex cs(mDecoderTaskMutex);
        if(mEndOfStreamFlag)
        {
            return true;
        }
    }

    // Partners should set this flag when the decoder has enough buffer
    return true;
}

// If the AudioESPlayer is enabled and the playback state is play, we are
// probably coming out of an on-the-fly audio switch. In this case, set playback
// to pending so that we don't actually try to start rendering media until we
// are sure we've buffered enough decoded frames that we are unlikely to
// immediately underflow at the renderer. Each time through the decoder loop we
// will check whether we are ready to move out of the pending state and begin
// playing back.
void AgrippinaAudioESPlayer::enable()
{
	AgrippinaESPlayer::enable();
}

void AgrippinaAudioESPlayer::flush()
{

    if(mPlaybackGroup->getPlaybackState() ==  IPlaybackGroup::PLAY)
    {
        NTRACE(TRACE_MEDIACONTROL,
                   "On-the-fly audio track switch in progress, setPlaybackPending");
        setPlaybackPending(true);
    }

    beginFlush();
    mPlaybackGroup->audioFlushed();
    endFlush();
}

void AgrippinaAudioESPlayer::beginFlush()
{
	m_NeroAudioDecoder->Flush();
    mIsFlushing = true;
}

void AgrippinaAudioESPlayer::endFlush()
{
    mIsFlushing = false;
}

bool AgrippinaAudioESPlayer::inputsAreExhausted()
{
    if(mInputExhausted)
    {
        return true;
    }
    return false;
}

MediaType AgrippinaAudioESPlayer::getMediaType()
{
    return AUDIO;
}

void AgrippinaAudioESPlayer::underflowReporter()
{
    if(!playbackPending())
    {
    	AgrippinaESPlayer::underflowReporter();
    }
}

bool AgrippinaAudioESPlayer::isDisabledOrPending()
{
     ScopedMutex cs(mDecoderTaskMutex);
     return mDisabled || mPlaybackPending;
}

bool AgrippinaAudioESPlayer::playbackPending()
{
    ScopedMutex cs(mDecoderTaskMutex);
    return mPlaybackPending;
}

void AgrippinaAudioESPlayer::setPlaybackPending(bool val)
{
    ScopedMutex cs(mDecoderTaskMutex);
    mPlaybackPending = val;
}
