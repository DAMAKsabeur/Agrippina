/*
 * (c) 1997-2016 Netflix, Inc.  All content herein is protected by
 * U.S. copyright and other applicable intellectual property laws and
 * may not be copied without the express permission of Netflix, Inc.,
 * which reserves all rights.  Reuse of any of this content for any
 * purpose without the permission of Netflix, Inc. is strictly
 * prohibited.
 */

#ifndef DEVICE_SYSTEMAUDIO_MANAGER_H
#define DEVICE_SYSTEMAUDIO_MANAGER_H

#include <nrd/ISystem.h>

namespace netflix {
namespace device {

class SystemAudioManager
{
public:
    SystemAudioManager();
    ~SystemAudioManager();

    netflix::VolumeControlType  getVolumeControlType();
    double                      getVolume();
    void                        setVolume(double volume);
    double                      getCurrentMinimumVolumeStep();
    bool                        isMuted();
    void                        setMute(bool muteSetting);
};

}}

#endif




