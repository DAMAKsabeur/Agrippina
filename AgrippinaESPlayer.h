/*
 * (c) 1997-2016 Netflix, Inc.  All content herein is protected by
 * U.S. copyright and other applicable intellectual property laws and
 * may not be copied without the express permission of Netflix, Inc.,
 * which reserves all rights.  Reuse of any of this content for any
 * purpose without the permission of Netflix, Inc. is strictly
 * prohibited.
 */
#ifndef AGRIPIPINA_ESPLAYER_H
#define AGRIPIPINA_ESPLAYER_H

/** @file AgrippinaESPlayer.h -  Reference implementation base class for
 * the IElementaryStreamPlayer interface.
 *
 * AgrippinaESPlayer is a base class for the reference application's implementation
 * of the IElementaryStreamPlayer interface.  A device partner may use or modify
 * this header and accomanying .cpp as needed.
 *
 * Stream player functionality that is common to both the audio and video
 * players is implemented in this base class.
 */

#include <nrd/IElementaryStreamPlayer.h>
#include <nrdbase/tr1.h>
#include "ESPlayerConstants.h"
#include <nrdbase/Thread.h>
#include <nrdbase/Mutex.h>
#include <nrdbase/NFError.h>
#include <nrdbase/NFErr.h>

#include <nrd/DrmManager.h>
#include <nrd/IDrmSession.h>

#include "DecoderStatsCounter.h"

namespace netflix {
namespace device {
namespace esplayer {

class AgrippinaPlaybackGroup;
class AgrippinaSampleWriter;

class NRDP_EXPORTABLE AgrippinaESPlayer : public IElementaryStreamPlayer
{
public:
    AgrippinaESPlayer();
    virtual ~AgrippinaESPlayer();

    enum FlushState { FLUSH_OFF, FLUSH_BEGIN, FLUSH_END };

    virtual NFErr init(const struct StreamPlayerInitData& initData,
                       std::shared_ptr<IESPlayerCallback> callback,
                       AgrippinaPlaybackGroup* playbackGroup);

    // IElementaryStreamPlayer methods.
    virtual void flush() = 0;
    virtual void close();
    virtual void disable();
    virtual void enable();
    virtual bool isDisabled();
    virtual void setVolume(double target, uint32_t duration, IAudioMixer::Ease ease);
    virtual double getVolume();


    // Methods overridden by audio and video players
    virtual void DecoderTask() = 0;
    virtual void EventTask() = 0;
    virtual bool inputsAreExhausted() = 0;
    virtual void beginFlush() = 0;
    virtual void endFlush() = 0;
    virtual MediaType getMediaType() = 0;

    virtual void setVideoPlaneProperties(VideoPlaneProperties);
    virtual VideoPlaneProperties getVideoPlaneProperties();
    virtual VideoPlaneProperties getPendingVideoPlaneProperties();
    virtual void commitVideoPlaneProperties();

    virtual void underflowReporter();
    virtual void updatePlaybackPosition(llong& PTS);
    virtual bool readyForPlaybackStart() = 0;

    virtual void setRenderMode(bool);
    virtual bool getRenderMode();

    virtual bool updateTexture(void *, int *, int *, bool);

    std::shared_ptr<IESPlayerCallback> getCallback();

protected:
    std::shared_ptr<IESPlayerCallback> mCallback;
    AgrippinaPlaybackGroup*          mPlaybackGroup;
    std::shared_ptr<IDrmSession> mDrmSession;
    std::string mContentId;

    // mCloseThreads and mDisabled are accessed from an SDK thread and the
    // decoderTask thread. mEndOfStreamFlag is accessed from the decoderTask and
    // renderer threads. These should be Synchronized with the
    // mDecoderTaskMutex.
    bool mCloseThreads;
    bool mEndOfStreamFlag;
    bool mDisabled;
    bool mRenderMode;
    double mCurrentVolume;
    Mutex mDecoderTaskMutex;
    uint32_t mPipelineId;
    std::stringstream mPipelineIdString;

    template <class _OBJECT_> void setFlushAndWait(_OBJECT_& object);

    NFErr decrypt(const ISampleAttributes *pSampleAttr,
                  AgrippinaSampleWriter& sampleWriter);

    bool closeThreadsFlagIsSet();
    void setCloseThreadsFlag();

    void reportDecodeOutcome(DecoderStatsCounter& stats);
};

template <typename _OBJECT_>
void AgrippinaESPlayer::setFlushAndWait(_OBJECT_& object)
{
    // to trigger the flush operation to begin
    object->setFlushState(FLUSH_BEGIN);

    // wait for the flush operation to end
    while(object->getFlushState() != FLUSH_END)
    {
        Thread::sleep(ESPlayerConstants::WAIT_FOR_FLUSH_END);
    }
}

} // namespace esplayer
} // device
} // namespace netflix

#endif /*AGRIPIPINA_ESPLAYER_H*/
