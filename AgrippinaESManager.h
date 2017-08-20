/*
 * (c) 1997-2016 Netflix, Inc.  All content herein is protected by
 * U.S. copyright and other applicable intellectual property laws and
 * may not be copied without the express permission of Netflix, Inc.,
 * which reserves all rights.  Reuse of any of this content for any
 * purpose without the permission of Netflix, Inc. is strictly
 * prohibited.
 */
#ifndef ESMANAGER_H
#define ESMANAGER_H

/** @file AgrippinaESManager.h - Reference implementation of the
 * IElementaryStreamManager interface.
 *
 * AgrippinaESManager is the reference application's implementation of the
 * IElementaryStreamManager interface (see IElementaryStreamPlayer.h). A
 * device partner may use or modify this header and accompanying .cpp
 * as needed.
 *
 * In the reference application, AgrippinaDeviceLib (the reference application's
 * implementation of IDeviceLib) creates one instance of the AgrippinaESManager when
 * the AgrippinaDeviceLib is constructed. The AgrippinaESManager instance persists
 * until the AgrippinaDeviceLib is destroyed when the application exits. As
 * specified in IElementaryStreamManager interface, AgrippinaESManager provides
 * methods to create playback groups and provides DRM functionality like
 * generating challenges and storing keys.  In the reference implementation, the
 * manager also provides a RendererManager that reference implementation uses to
 * provide video and audio players access to the Linux-specific video and audio
 * rendering plugins.
 *
 * The DPI specifies that playback groups are only created by the
 * IElementaryStreamManager object and that elementary stream players are in
 * turn only created by IPlaybackGroup so that the device can use the manager to
 * track all the playback resources in use.  The reference implementation gives
 * each playback group access to the manager object for use when audio and video
 * renderers need to be allocated.
 **/

#include <nrd/IElementaryStreamPlayer.h>
#include <nrd/ISystem.h>
#include <nrd/DrmManager.h>

#include <nrdbase/tr1.h>
#include <nrdbase/NFErr.h>
#include <nrdbase/Mutex.h>
#include <set>

#include "AgrippinaPlaybackGroup.h"
#include "ESPlayerConstants.h"

namespace netflix {
namespace device {
namespace esplayer {

class AgrippinaESManager : public IElementaryStreamManager
{
public:
    AgrippinaESManager();
    virtual ~AgrippinaESManager();

    /**
     * IElementaryStreamManager methods
     */
    virtual NFErr createPlaybackGroup(IPlaybackGroup*& playbackGroup); // to be deprecated
    virtual NFErr createPlaybackGroup(IPlaybackGroup*& playbackGroup, uint32_t pipelineId = 0);
    virtual void destroyPlaybackGroup(IPlaybackGroup* streamPlayer);
    virtual void commitPlaybackGroups();

    /*
     *  methods
     */
    netflix::DrmManager* getMultiSessionDrmManager();
    
private:
    std::set<AgrippinaPlaybackGroup*> mPlaybackGroups;
    netflix::DrmManager*           mMultiSessionDrmManager;
    std::map<uint32_t, bool>       mIsMediaPipelineInUse;
    Mutex                          mPlaybackGroupsMutex;
};

} // namespace esplayer
} // namespace netflix
} // namespace device

#endif
