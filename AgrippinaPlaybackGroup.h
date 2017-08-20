/*
 * (c) 1997-2016 Netflix, Inc.  All content herein is protected by
 * U.S. copyright and other applicable intellectual property laws and
 * may not be copied without the express permission of Netflix, Inc.,
 * which reserves all rights.  Reuse of any of this content for any
 * purpose without the permission of Netflix, Inc. is strictly
 * prohibited.
 */
#ifndef AGRIPIPINA_PLAYBACKGROUP_H
#define AGRIPIPINA_PLAYBACKGROUP_H

/** @file AgrippinaPlaybackGroup.h - Reference implementation of the IPlaybackGroup
 * interface.
 *
 * AgrippinaPlaybackGroup is the reference application's implementation of the
 * IPlaybackGroup interface (see IElementaryStreamPlayer.h). A device partner
 * may use or modify this header and accompanying .cpp as needed.
 *
 * A AgrippinaPlaybackGroup is created each time a movie is loaded for viewing and
 * persists until the movie is unloaded. The playback group is used to
 * instantiate video and audio elementary stream players,to set the playback
 * state of the movie presentation to play or pause, and to control the video
 * window as defined in IElementaryStreamPlayer.h. AgrippinaPlaybackGroup has an
 * instance of ReferenceClock which stores the current playback positions of the
 * audio and video players.
 *
 */

#include <nrd/ISystem.h>
#include <nrd/IElementaryStreamPlayer.h>
#include <nrd/IDeviceError.h>
#include <nrdbase/tr1.h>
#include <nrdbase/Mutex.h>
#include <nrdbase/NFErr.h>
#include <set>
/* include from nero include dir */
#include "NeroSTC.h"
#include "NeroAudioDecoder.h"
#include "NeroVideoDecoder.h"
#include "NeroConstants.h"

namespace netflix {
namespace device {
namespace esplayer {


class AgrippinaESManager;
class AgrippinaESPlayer;
class AgrippinaAudioESPlayer;
class AgrippinaVideoESPlayer;


class AgrippinaPlaybackGroup : public IPlaybackGroup
{

public:
    virtual ~AgrippinaPlaybackGroup();
    AgrippinaPlaybackGroup(AgrippinaESManager& ESManager, uint32_t pipelineId);

    virtual netflix::NFErr
    createStreamPlayer(const struct StreamPlayerInitData& initData,
                       std::shared_ptr<IESPlayerCallback> callback,
                       IElementaryStreamPlayer*& pStreamPlayer);
    virtual void destroyStreamPlayer(IElementaryStreamPlayer* streamPlayer);
    virtual bool setPlaybackState(PlaybackState state);
    virtual IPlaybackGroup::PlaybackState getPlaybackState();
    virtual void flush();

    virtual void setVideoPlaneProperties(VideoPlaneProperties);
    virtual VideoPlaneProperties getVideoPlaneProperties();
    virtual VideoPlaneProperties getPendingVideoPlaneProperties();
    void commitVideoPlaneProperties();

    bool getRenderMode();

    AgrippinaESManager* getESManager();

    // The audio player, if it is flushed independent of the playback group, calls
    // this method so that the reference clock's current audio presentation time
    // can be invalidated.
    void audioFlushed();
    uint32_t getPipelineId() const;

private:
    bool streamPlayersAreReadyForPlaybackStart();

    // The video audio rendering threads access mPlaybackState. The state is
    // set in and SDK thread. The mPlaybackStateMutex should be held when
    // mPlaybackState is accessed.
    IPlaybackGroup::PlaybackState   mPlaybackState;
    Mutex                           mPlaybackStateMutex;
    Mutex                           mPlayersMutex;

    AgrippinaESManager&                    mESManager;
    std::set<AgrippinaESPlayer*>           mStreamPlayers;
    uint32_t mPipelineId;
    NeroSTC* stc;
    NeroAudioDecoder* m_NeroAudioDecoder;
    NeroVideoDecoder* m_NeroVideoDecoder;
};

} // namespace esplayer
} // device
} // namespace netflix



#endif /* AGRIPIPINA_PLAYBACKGROUP_H*/

