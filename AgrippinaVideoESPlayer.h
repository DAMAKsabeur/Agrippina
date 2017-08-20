/*
 * (c) 1997-2016 Netflix, Inc.  All content herein is protected by
 * U.S. copyright and other applicable intellectual property laws and
 * may not be copied without the express permission of Netflix, Inc.,
 * which reserves all rights.  Reuse of any of this content for any
 * purpose without the permission of Netflix, Inc. is strictly
 * prohibited.
 */
#ifndef AGRIPIPINA_VIDEOESPLAYER_H
#define AGRIPIPINA_VIDEOESPLAYER_H

/** @file AgrippinaVideoESPlayer.h - Reference implementation of the video
 * IElementaryStreamPlayer.
 *
 * AgrippinaVideoESPlayer is the reference application's implementation of a video
 * player. It implements the IElementaryStreamPlayer interface (see
 * IElementaryStreamPlayer.h). A device partner may use or modify this header
 * and accompanying .cpp as needed.
 *
 * A AgrippinaVideoESPlayer is created by the
 * AgrippinaPlaybackGroup::createStreamPlayer() method each time a movie is loaded
 * for viewing and persists until the movie is unloaded. When it's created, the
 * player spawns a decoder thread and a renderer thread. The decoder thread uses
 * the IESPlayerCallback object the player receives when it is created to pull
 * video samples (also known as access units in H.264/AVC) from the netlix
 * application. When it receives a sample, it decrypts it as needed, then
 * decodes the frame and adds it to the renderer's input queue. The renderer
 * thread uses the AgrippinaPlaybackGroup's ReferenceClock and the PTS timestamps
 * on the decoded frames to determine when to remove decoded frames from its
 * input queue and write them to the video driver.
 *
 * In the reference application, there there are several different options for
 * the video renderer that is used. The VideoESPlayer uses the ESManager (which
 * is available via the AgrippinaPlaybackGroup) to get the RendererManager that
 * provides the video renderer that was selected at run time.
 */

#include "AgrippinaESPlayer.h"
#include "AgrippinaSampleWriter.h"
/* include from nero include dir */
#include "NeroSTC.h"
#include "NeroVideoDecoder.h"
#include "NeroConstants.h"

namespace netflix {
namespace device {
namespace esplayer {


class DeviceThread;

class AgrippinaVideoESPlayer :  public AgrippinaESPlayer
{
public:

	AgrippinaVideoESPlayer(NeroVideoDecoder* NeroVideoDecoder_ptr);
    virtual ~AgrippinaVideoESPlayer();

    virtual NFErr init(const struct StreamPlayerInitData& initData,
                       std::shared_ptr<IESPlayerCallback> callback,
                       AgrippinaPlaybackGroup* playbackGroup);
    virtual void flush();
    virtual void close();
    virtual void decoderTask();
    virtual bool inputsAreExhausted();
    virtual MediaType getMediaType();
    virtual bool readyForPlaybackStart();

    virtual void setVideoPlaneProperties(VideoPlaneProperties);
    virtual VideoPlaneProperties getVideoPlaneProperties();
    virtual VideoPlaneProperties getPendingVideoPlaneProperties();
    virtual void commitVideoPlaneProperties();

    virtual void setRenderMode(bool texture);
    virtual bool getRenderMode();
    virtual bool updateTexture(void *, int *, int *, bool);

private:
  friend class AgrippinaPlaybackGroup;
    Nero_video_codec_t Nrd2NeroVideoCodec_Remap(const char* nrd_video_codec);
    AgrippinaVideoESPlayer();

    virtual void beginFlush();
    virtual void endFlush();

    void updateTransition();
    void log3DType(Format3D format3D);
    void handleFrameDropReporting(const llong& refTimestamp,
                                  const llong& picTimestamp);
    
    std::shared_ptr<DeviceThread>          mVideoDecoderThread;
    std::shared_ptr<AgrippinaSampleWriter>    mSampleWriter;
    Format3D                          mCurrent3DFormat;
    bool                              mVP9;

    // Location of the window right now
    Rect        mCurrentVideoWindow;
    float       mCurrentAlpha;
    uint32_t    mCurrentZOrder;

    // Video plane properties to be applied
    Rect        mCommitVideoWindow;
    float       mCommitAlpha;
    uint32_t    mCommitZOrder;

    // Pending to be committed
    Rect        mPendingVideoWindow;
    float       mPendingAlpha;
    uint32_t    mPendingZOrder;

    // Keep track of timestamps and rendering times so we can log when the
    // renderer drops a frame.
    llong mPrevPicTimestamp;
    llong mPrevRefTimestamp;

    llong mTsOfLastFrameDeliveredToRenderer;

    // setVideoPlaneProperties set some properties
    bool    mPendingSet;

    // Debugging
    bool mFileDumping;
    bool mFileDumpingOnlyGop;
    std::string mFilename;
    FILE* mFp;

    bool mInputExhausted;
    bool mIsFlushing;

    NeroVideoDecoder* m_NeroVideoDecoder;
};

} // esplayer
} // namespace device
} // namespace netflix

#endif /* AGRIPIPINA_VIDEOESPLAYER_H */
