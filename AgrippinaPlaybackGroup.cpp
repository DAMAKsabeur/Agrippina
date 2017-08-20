/*
 * (c) 1997-2016 Netflix, Inc.  All content herein is protected by
 * U.S. copyright and other applicable intellectual property laws and
 * may not be copied without the express permission of Netflix, Inc.,
 * which reserves all rights.  Reuse of any of this content for any
 * purpose without the permission of Netflix, Inc. is strictly
 * prohibited.
 */
#include "AgrippinaPlaybackGroup.h"
#include "ESPlayerConstants.h"
#include "AgrippinaESManager.h"
#include "AgrippinaAudioESPlayer.h"
#include "AgrippinaVideoESPlayer.h"
#include "AgrippinaDeviceLib.h"
#include <nrd/IPlaybackDevice.h>
#include <nrdbase/Log.h>
#include <nrdbase/ScopedMutex.h>
#include <nrdbase/MutexRanks.h>
#include <nrd/NrdApplication.h>

using namespace netflix;
using namespace netflix::device::esplayer;
using namespace netflix::device;
using namespace std;

static uint32_t videoPlayerCount = 0;

AgrippinaPlaybackGroup::AgrippinaPlaybackGroup(AgrippinaESManager& ESManager, uint32_t pipelineId) :
  mPlaybackState(PAUSE),
  mPlaybackStateMutex(ESP_PLAYBACKGROUP_MUTEX, "ESP Playback Group Mutex"),
  mPlayersMutex(UNTRACKED_MUTEX, "ESP Players Mutex"),
  mESManager(ESManager),
  mPipelineId(pipelineId)
{
	stc = new NeroSTC(0x01);
	stc->NeroSTCSetType(NERO_STC_VIDEO_MASTER);
	m_NeroAudioDecoder = new  NeroAudioDecoder(stc);
	m_NeroVideoDecoder = new  NeroVideoDecoder(stc);
	m_NeroVideoDecoder->SetupPlane();
}

AgrippinaPlaybackGroup::~AgrippinaPlaybackGroup()
{
    ScopedMutex cs(mPlayersMutex);
    // Delete any stream players that have not been deleted
    delete stc;
    delete m_NeroAudioDecoder;
    delete m_NeroVideoDecoder;
    stc = NULL;
    m_NeroAudioDecoder = NULL;
    m_NeroVideoDecoder = NULL;
    set<AgrippinaESPlayer*>::iterator it;
    for (it = mStreamPlayers.begin(); it != mStreamPlayers.end(); it++) {
        delete *it;
    }
}

NFErr
AgrippinaPlaybackGroup::createStreamPlayer(const struct StreamPlayerInitData& initData,
                                        std::shared_ptr<IESPlayerCallback> callback,
                                        IElementaryStreamPlayer*& pStreamPlayer)
{
    NFErr err;
    ullong deviceSpecificErrorCode = 0;
    std::string deviceSpecificErrorMsg;
    std::string deviceStackTraceMsg;

    AgrippinaESPlayer* player = NULL;
    pStreamPlayer = NULL;

    // Check initData pointers.
    if(initData.mInitialStreamAttributes == NULL) {
        assert(initData.mInitialStreamAttributes != NULL);

        //this should be replaced by partner specific error code
        deviceSpecificErrorCode = AgrippinaAppDeviceSpecific_NoInitialStreamAttributes;
        Variant errInfo;
        errInfo["errorDescription"] = "initial stream attributes NULL";
        return err.push(new IDeviceError(INITIALIZATION_ERROR,
                                         deviceSpecificErrorCode,
                                         errInfo));
    }

    MediaType streamType = initData.mInitialStreamAttributes->mStreamType;

    // Initialize the audio player if this is an audio stream.
    if(streamType == AUDIO)
    {
        // Check audio attributes pointer
        assert(initData.mInitialStreamAttributes->mAudioAttributes != NULL);
        if(initData.mInitialStreamAttributes->mAudioAttributes == NULL)
        {
            // this should be replaced by partner specific error code
            deviceSpecificErrorCode = AgrippinaAppDeviceSpecific_NoInitialAudioStreamAttributes;
            deviceSpecificErrorMsg = "no initialStreamAttributes for Audio";
            Variant errInfo;
            errInfo["errorDescription"] = "no initialStreamAttributes for Audio";
            return err.push(new IDeviceError(INITIALIZATION_ERROR,
                                             deviceSpecificErrorCode,
                                             errInfo));
        }
        player = new AgrippinaAudioESPlayer(m_NeroAudioDecoder);


    } else {
        // Initialize a video player
        // Check video attributes pointer.
        assert(initData.mInitialStreamAttributes->mVideoAttributes != NULL);
        if(initData.mInitialStreamAttributes->mVideoAttributes == NULL)
        {
            // this should be replaced by partner specific error code
            deviceSpecificErrorCode = AgrippinaAppDeviceSpecific_NoInitialVideoStreamAttributes;
            Variant errInfo;
            errInfo["errorDescription"] = "no initialStreamAttributes for Video";
            return err.push(new IDeviceError(INITIALIZATION_ERROR,
                                             deviceSpecificErrorCode,
                                             errInfo));
        }

        // If a device requires MVC delivered with the base and depenent view
        // NALUs in separate buffers, USE_MVC_SPLIT should be defined.  See
        // IElementaryStreamPlayer.h for details about MVC_SPLIT and
        // MVC_COMBINED mode.
// #define USE_MVC_SPLIT
#ifdef USE_MVC_SPLIT
        if(initData.mInitialStreamAttributes->mVideoAttributes->mFormat3D == MVC_COMBINED)
        {
            // this should be replaced by partner specific error code
            deviceSpecificErrorCode = skeletonDeviceSpecific_Wrong3DMode;
            Variant errInfo;
            errInfo["errorDescription"] = "wrong 3D stream architecture";
            return err.push(new IDeviceError(SPLIT_MVC_REQUIRED,
                                             deviceSpecificErrorCode,
                                             errInfo));
        }
#endif

        if (streamType == VIDEO) {
            uint32_t maxVideoPipelines = 1;
            ScopedMutex cs(mPlayersMutex);
            if (videoPlayerCount < maxVideoPipelines) {
                ++videoPlayerCount;
            } else {
                NTRACE(TRACE_MEDIAPLAYBACK, "Cannot create more than %d video players at a time", maxVideoPipelines);

                deviceSpecificErrorCode = AgrippinaAppDeviceSpecific_VideoResourceError;
                Variant errInfo;
                errInfo["errorDescription"] = "platform does not have enough resource";
                return err.push(new IDeviceError(INITIALIZATION_ERROR,
                                                 deviceSpecificErrorCode,
                                                 errInfo));
            }
        }

        player = new AgrippinaVideoESPlayer(m_NeroVideoDecoder);
    }

    if(player == NULL)
    {
        if(streamType == AUDIO){
            deviceSpecificErrorCode = AgrippinaAppDeviceSpecific_AudioPlayerCreationFailed;
        } else {
            deviceSpecificErrorCode = AgrippinaAppDeviceSpecific_VideoPlayerCreationFailed;
        }
        Variant errInfo;
        errInfo["errorDescription"] = "failed to create player";
        return err.push(new IDeviceError(INITIALIZATION_ERROR,
                                         deviceSpecificErrorCode,
                                         errInfo));
    }

    err = player->init(initData, callback, this);
    if(!err.ok()){
        delete player;
        return err;
    }

    NTRACE(TRACE_MEDIAPLAYBACK, "AgrippinaPlaybackGroup::createStreamPlayer: %s player initialized",
           streamType == AUDIO ? "audio" : "video");

    pStreamPlayer = player;

    ScopedMutex cs(mPlayersMutex);
    mStreamPlayers.insert(player);

    return err;
}

void AgrippinaPlaybackGroup::destroyStreamPlayer(IElementaryStreamPlayer* iesPlayer)
{
    AgrippinaESPlayer* streamPlayer = static_cast<AgrippinaESPlayer*>(iesPlayer);

    ScopedMutex cs(mPlayersMutex);
    assert(mStreamPlayers.find(streamPlayer) != mStreamPlayers.end());
    if (mStreamPlayers.find(streamPlayer) != mStreamPlayers.end()) {
        if(streamPlayer->getMediaType() == VIDEO) {
            --videoPlayerCount;
        }

        mStreamPlayers.erase(streamPlayer);
        delete streamPlayer;
    }
}

bool AgrippinaPlaybackGroup::streamPlayersAreReadyForPlaybackStart()
{
    set<AgrippinaESPlayer*>::iterator it;

    for(it = mStreamPlayers.begin(); it != mStreamPlayers.end(); it++)
    {
        if(!(*it)->readyForPlaybackStart())
        {
            return false;
        }
    }
    return true;
}

bool AgrippinaPlaybackGroup::setPlaybackState(PlaybackState state)
{
    NTRACE(TRACE_MEDIAPLAYBACK, "AgrippinaPlaybackGroup::setPlaybackState(): "
           "state = %s\n", state == PLAY ? "PLAY":"PAUSE");
    Nero_error_t result = NERO_SUCCESS;
    if(state == PAUSE)
    {
        // Partners should pause the pipeline
    	result = m_NeroAudioDecoder->Pause();
    	if (NERO_SUCCESS != result){
            // Players are not ready to transition from play to pause. Return
            // false.  The SDK will wait a few ms and try again.
            return false;
    	}
    	result = m_NeroVideoDecoder->Pause();
    	if (NERO_SUCCESS != result){
            // Players are not ready to transition from play to pause. Return
            // false.  The SDK will wait a few ms and try again.
            return false;
    	}

    }
    else if(state == PLAY)
    {
        if(!streamPlayersAreReadyForPlaybackStart() )
        {
            // Players are not ready to transition from paused to play. Return
            // false.  The SDK will wait a few ms and try again.
            return false;
        }
        // Partners should play the pipeline
    	result = m_NeroAudioDecoder->Play();
    	if (NERO_SUCCESS != result){
            // Players are not ready to transition from play to pause. Return
            // false.  The SDK will wait a few ms and try again.
            return false;
    	}
    	result = m_NeroVideoDecoder->Play();
    	if (NERO_SUCCESS != result){
            // Players are not ready to transition from play to pause. Return
            // false.  The SDK will wait a few ms and try again.
            return false;
    	}
    }
    else {
        // We only support PAUSE and PLAY.  If a new state is added make sure handling gets implemented here!
        assert(0);
    }

    {
        ScopedMutex cs(mPlaybackStateMutex);
        mPlaybackState = state;
    }
    return true;
}

IPlaybackGroup::PlaybackState AgrippinaPlaybackGroup::getPlaybackState()
{
    ScopedMutex cs(mPlaybackStateMutex);
    return mPlaybackState;
}

void AgrippinaPlaybackGroup::flush()
{
    set<AgrippinaESPlayer*>::iterator it;
    NTRACE(TRACE_MEDIAPLAYBACK, "AgrippinaPlaybackGroup::flush() begin");

    // Partners should pause the pipeline

    for(it = mStreamPlayers.begin(); it != mStreamPlayers.end(); it++)
    {
        (*it)->beginFlush();
    }

    // Partners should invalidate the presentation time (if needed)

    for(it = mStreamPlayers.begin(); it != mStreamPlayers.end(); it++)
    {
        (*it)->endFlush();
    }

    NTRACE(TRACE_MEDIAPLAYBACK, "AgrippinaPlaybackGroup::flush() end");

}

void AgrippinaPlaybackGroup::audioFlushed()
{
    // Partners should invalidate the presentation time (if needed)
}

void AgrippinaPlaybackGroup::setVideoPlaneProperties(VideoPlaneProperties p)
{
    ScopedMutex cs(mPlayersMutex);

    set<AgrippinaESPlayer*>::iterator it;
    for (it = mStreamPlayers.begin(); it != mStreamPlayers.end(); it++) {
        if ((*it)->getMediaType() == VIDEO) {
            (*it)->setVideoPlaneProperties(p);
            return;
        }
    }
}

VideoPlaneProperties AgrippinaPlaybackGroup::getVideoPlaneProperties()
{
    ScopedMutex cs(mPlayersMutex);

    set<AgrippinaESPlayer*>::iterator it;
    for (it = mStreamPlayers.begin(); it != mStreamPlayers.end(); it++) {
        if ((*it)->getMediaType() == VIDEO) {
            return (*it)->getVideoPlaneProperties();
        }
    }

    VideoPlaneProperties p = VideoPlaneProperties();
    p.zorder = UINT_MAX;

    return p;
}

VideoPlaneProperties AgrippinaPlaybackGroup::getPendingVideoPlaneProperties()
{
    ScopedMutex cs(mPlayersMutex);

    set<AgrippinaESPlayer*>::iterator it;
    for (it = mStreamPlayers.begin(); it != mStreamPlayers.end(); it++) {
        if ((*it)->getMediaType() == VIDEO) {
            return (*it)->getPendingVideoPlaneProperties();
        }
    }

    VideoPlaneProperties p = VideoPlaneProperties();
    p.zorder = UINT_MAX;
    return p;
}

void AgrippinaPlaybackGroup::commitVideoPlaneProperties()
{
    ScopedMutex cs(mPlayersMutex);

    set<AgrippinaESPlayer*>::iterator it;
    for (it = mStreamPlayers.begin(); it != mStreamPlayers.end(); it++) {
        if ((*it)->getMediaType() == VIDEO) {
            (*it)->commitVideoPlaneProperties();
        }
    }
}

bool AgrippinaPlaybackGroup::getRenderMode()
{
    ScopedMutex cs(mPlayersMutex);

    set<AgrippinaESPlayer*>::iterator it;
    for (it = mStreamPlayers.begin(); it != mStreamPlayers.end(); it++) {
        if ((*it)->getMediaType() == VIDEO) {
            return (*it)->getRenderMode();
        }
    }

    return false;
}

AgrippinaESManager* AgrippinaPlaybackGroup::getESManager()
{
    return &mESManager;
}

uint32_t AgrippinaPlaybackGroup::getPipelineId() const
{
    return mPipelineId;
}
