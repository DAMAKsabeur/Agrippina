/*
 * (c) 1997-2016 Netflix, Inc.  All content herein is protected by
 * U.S. copyright and other applicable intellectual property laws and
 * may not be copied without the express permission of Netflix, Inc.,
 * which reserves all rights.  Reuse of any of this content for any
 * purpose without the permission of Netflix, Inc. is strictly
 * prohibited.
 */
#ifndef ESPLAYERCONSTANTS_H
#define ESPLAYERCONSTANTS_H

/* @file ESPlayerConstants.h - Contains constants specific to the reference
 * implementation of the IElementaryStreamPlayer interface.  A device
 * implementation may modify this header and accompanying .cpp files as
 * needed.
 */

#include <nrdbase/Thread.h>
#include <nrdbase/Time.h>
#include <nrddpi/Common.h>
#include <nrdbase/Exportable.h>

namespace netflix  {
namespace device   {
namespace esplayer {

class NRDP_EXPORTABLE ESPlayerConstants
{
public:
    // ES Player Settings
    static const Time WAIT_FOR_FLUSH_OFF;
    static const Time WAIT_FOR_FLUSH_END;
};

/*
 * Partner devices should use device specific code that is meaningful for target
 * device.
 */
enum AgrippinaAppDeviceSpecificError
{
    AgrippinaAppDeviceSpecific_NoError = 0,

    // related to drm
    AgrippinaAppDeviceSpecific_UnknownPrectionSystemId = 1000,
    AgrippinaAppDeviceSpecific_NoDecryptionContext,
    AgrippinaAppDeviceSpecific_WrongSubsampleMapping,
    AgrippinaAppDeviceSpecific_NoDrmSystemFound,
    AgrippinaAppDeviceSpecific_NoDrmSessionFound,
    AgrippinaAppDeviceSpecific_InvalidDrmSecureStopId,
    AgrippinaAppDeviceSpecific_InvalidVideoResolution,

    // related to decoding
    AgrippinaAppDeviceSpecific_WrongBitDepth = 2000,
    AgrippinaAppDeviceSpecific_MissingSampleAttributes,
    AgrippinaAppDeviceSpecific_UpdateAttributesFailed,
    AgrippinaAppDeviceSpecific_HevcDecoderError,
    AgrippinaAppDeviceSpecific_FfmpegDecoderError,

    // related to player
    AgrippinaAppDeviceSpecific_NoInitialStreamAttributes = 3000,
    AgrippinaAppDeviceSpecific_NoInitialAudioStreamAttributes,
    AgrippinaAppDeviceSpecific_NoInitialVideoStreamAttributes,
    AgrippinaAppDeviceSpecific_VideoPlayerCreationFailed,
    AgrippinaAppDeviceSpecific_VideoPlayerInitializationFailed,
    AgrippinaAppDeviceSpecific_AudioPlayerCreationFailed,
    AgrippinaAppDeviceSpecific_AudioPlayerInitializationFailed,
    AgrippinaAppDeviceSpecific_Wrong3DMode,
    AgrippinaAppDeviceSpecific_Wrong3DViewNumber,
    AgrippinaAppDeviceSpecific_createPlaybackGroupFailed,

    // video resource
    AgrippinaAppDeviceSpecific_VideoResourceError = 4000,
    AgrippinaAppDeviceSpecific_DrmNotSupportedByPipeline,
};

} // namespace esplayer
} // namespace device
} // namespace netflix

#endif
