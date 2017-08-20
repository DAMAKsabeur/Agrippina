/*
 * (c) 1997-2016 Netflix, Inc.  All content herein is protected by
 * U.S. copyright and other applicable intellectual property laws and
 * may not be copied without the express permission of Netflix, Inc.,
 * which reserves all rights.  Reuse of any of this content for any
 * purpose without the permission of Netflix, Inc. is strictly
 * prohibited.
 */

#include <nrd/DeviceConstants.h>
#include <nrd/IDeviceError.h>
#include <nrd/NrdApplication.h>
#include <nrd/DrmManager.h>

#include "AgrippinaESManager.h"
#include "AgrippinaPlaybackGroup.h"

using namespace netflix;
using namespace netflix::device;
using namespace netflix::device::esplayer;
using namespace std;

AgrippinaESManager::AgrippinaESManager() :
    mPlaybackGroupsMutex(UNTRACKED_MUTEX, "PlaybackGroupsMutex")
{
    mMultiSessionDrmManager = netflix::DrmManager::instance();
    mIsMediaPipelineInUse.clear();
}


AgrippinaESManager::~AgrippinaESManager()
{
    // Delete any stream players that have not been deleted
    set<AgrippinaPlaybackGroup*>::iterator it;
    for(it = mPlaybackGroups.begin(); it != mPlaybackGroups.end(); it++)
    {
        delete *it;
    }
    mIsMediaPipelineInUse.clear();
}

NFErr AgrippinaESManager::createPlaybackGroup(IPlaybackGroup*& playbackGroup)
{
    return createPlaybackGroup(playbackGroup, 0);
}

NFErr AgrippinaESManager::createPlaybackGroup(IPlaybackGroup*& playbackGroup, uint32_t pipelineId)
{
    NTRACE(TRACE_MEDIAPLAYBACK
           , "AgrippinaESManager::createPlaybackGroup with pipelineId %d"
           , pipelineId);

    ScopedMutex lock(mPlaybackGroupsMutex);
    if(mPlaybackGroups.size() == 0){
        uint32_t numPipelines = 1;
        for(uint32_t i=0; i<numPipelines; i++){
            mIsMediaPipelineInUse[i] = false; // false means, not in use. true means in use.
        }
    }

    NFErr err;
    ullong deviceSpecificErrorCode = AgrippinaAppDeviceSpecific_createPlaybackGroupFailed;

    if(mIsMediaPipelineInUse[pipelineId] == true){
        // disable until nrdjs passes correct pipelineId
        /*
        std::ostringstream ss;
        ss << "AgrippinaESManager::createPlaybackGroup failed : pipeline already in use";
        Variant errInfo;
        errInfo["errorDescription"] = ss.str();
        err.push(new IDeviceError(INITIALIZATION_ERROR,
                                  deviceSpecificErrorCode,
                                  errInfo));
        return err;
        */
    }

    AgrippinaPlaybackGroup* group = new AgrippinaPlaybackGroup(*this, pipelineId);
    if(group == NULL)
    {
        std::ostringstream ss;
        ss << "AgrippinaESManager::createPlaybackGroup failed to instantiate AgrippinaPlaybackGroup";
        Variant errInfo;
        errInfo["errorDescription"] = ss.str();
        err.push(new IDeviceError(INITIALIZATION_ERROR,
                                  deviceSpecificErrorCode,
                                  errInfo));
        return err;
    } else {
        mPlaybackGroups.insert(group);
        playbackGroup = group;
        mIsMediaPipelineInUse[pipelineId] = true; // false means, not in use. true means in use.
        return err;
    }
}

void AgrippinaESManager::destroyPlaybackGroup(IPlaybackGroup* playbackGroup)
{
    ScopedMutex lock(mPlaybackGroupsMutex);
    AgrippinaPlaybackGroup* group = static_cast<AgrippinaPlaybackGroup*>(playbackGroup);
    assert(mPlaybackGroups.find(group) != mPlaybackGroups.end());
    if(mPlaybackGroups.find(group) != mPlaybackGroups.end())
    {
        mIsMediaPipelineInUse[group->getPipelineId()] = false;
        mPlaybackGroups.erase(group);
        delete group;
    }
}

void AgrippinaESManager::commitPlaybackGroups()
{
    /*
        Note that this function for CE devices should not be required.
        CE devices are required to commit the pending properties on the
        graphics flip() call to synchronize graphics with video plane
        coordinates.  However, on reference application the renderer(s)
        run in different threads and update DFB windows independently of
        each other and of the graphics flip() call.
    */
    ScopedMutex lock(mPlaybackGroupsMutex);
    set<AgrippinaPlaybackGroup*>::iterator it;
    for (it = mPlaybackGroups.begin(); it != mPlaybackGroups.end(); ++it) {
        (*it)->commitVideoPlaneProperties();
    }

    uint32_t nonTexturePlayers = 0;

    /*
        Due to reference applications video renderers committing the pending
        properties at different times instead of atomically at graphics flip()
        time as required for CE devices we need to query the pending values
        to check for z-order conflict instead of actual values.
    */
    std::map<uint32_t, VideoPlaneProperties> properties;
    for (it = mPlaybackGroups.begin(); it != mPlaybackGroups.end(); ++it) {
        if ((*it)->getRenderMode() == false) {
            VideoPlaneProperties p = (*it)->getPendingVideoPlaneProperties();

            if (p.zorder != UINT_MAX) {
                properties[p.zorder] = p;
                ++nonTexturePlayers;
            }
        }
    }

    if (nonTexturePlayers != properties.size()) {
        Log::error(TRACE_MEDIAPLAYBACK, "Z-order conflict between video players");
    }

}

netflix::DrmManager* AgrippinaESManager::getMultiSessionDrmManager()
{
    return mMultiSessionDrmManager;
}

