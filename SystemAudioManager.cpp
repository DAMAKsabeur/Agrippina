/*
 * (c) 1997-2016 Netflix, Inc.  All content herein is protected by
 * U.S. copyright and other applicable intellectual property laws and
 * may not be copied without the express permission of Netflix, Inc.,
 * which reserves all rights.  Reuse of any of this content for any
 * purpose without the permission of Netflix, Inc. is strictly
 * prohibited.
 */
#include "SystemAudioManager.h"

#ifdef AUDIO_MIXER_OUTPUT_ALSA
#include "x86/SystemAudioALSA.h"
#endif

using namespace netflix;
using namespace netflix::device;

SystemAudioManager::SystemAudioManager()
{
#ifdef AUDIO_MIXER_OUTPUT_ALSA
    SystemAudioALSA::init();
#endif
}

SystemAudioManager::~SystemAudioManager()
{
#ifdef AUDIO_MIXER_OUTPUT_ALSA
    SystemAudioALSA::fini();
#endif
}

VolumeControlType SystemAudioManager::getVolumeControlType()
{
#ifdef AUDIO_MIXER_OUTPUT_ALSA
    return SystemAudioALSA::getVolumeControlType();
#else
    return NO_VOLUME_CONTROL;
#endif
}

double SystemAudioManager::getVolume()
{
#ifdef AUDIO_MIXER_OUTPUT_ALSA
    return SystemAudioALSA::getVolume();
#else
    return 1.0;
#endif
}

void SystemAudioManager::setVolume(double volume)
{
#ifdef AUDIO_MIXER_OUTPUT_ALSA
    SystemAudioALSA::setVolume(volume);
#else
    NRDP_UNUSED(volume);
#endif
}

double SystemAudioManager::getCurrentMinimumVolumeStep()
{
#ifdef AUDIO_MIXER_OUTPUT_ALSA
    return SystemAudioALSA::getCurrentMinimumVolumeStep();
#else
    return 0.01;
#endif
}

bool SystemAudioManager::isMuted()
{
#ifdef AUDIO_MIXER_OUTPUT_ALSA
    return SystemAudioALSA::isMuted();
#else
    return false;
#endif
}

void SystemAudioManager::setMute(bool muteSetting)
{
#ifdef AUDIO_MIXER_OUTPUT_ALSA
    SystemAudioALSA::setMute(muteSetting);
#else
    NRDP_UNUSED(muteSetting);
#endif
}
