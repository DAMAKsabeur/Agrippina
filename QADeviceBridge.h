/*
 * (c) 1997-2016 Netflix, Inc.  All content herein is protected by
 * U.S. copyright and other applicable intellectual property laws and
 * may not be copied without the express permission of Netflix, Inc.,
 * which reserves all rights.  Reuse of any of this content for any
 * purpose without the permission of Netflix, Inc. is strictly
 * prohibited.
 */

#ifndef QADEVICEBRIDGE_H
#define QADEVICEBRIDGE_H

/* @file QADeviceBridge.h - Used by netflix QA for SDK testing.
 */

#include <nrdbase/Variant.h>
#include <nrdbase/tr1.h>

#include <nrd/NfObject.h>

#ifdef NRDP_HAS_QA
#include "FileSystem.h"

#if 0
Method(isSubtitleSupported) \
Method(setSubtitleSupported) \
Method(configureWeb)
#endif

#define QADevice_Methods(Method)                \
    Method(configureModelGroupKey)              \
    Method(configurePreshared)                  \
    Method(getAuthenticationType)               \
    Method(getDolbyVisionRendering)             \
    Method(getIpConnectivityMode)               \
    Method(sendCommand)                         \
    Method(setActiveVideoOutput)                \
    Method(setCapability)                       \
    Method(setDolbyVisionFileDump)              \
    Method(setDolbyVisionRendering)             \
    Method(setIpConnectivityMode)               \
    Method(setLanguage)                         \
    Method(setProfiles)                         \
    Method(setSupportedVideoOutput)             \
    Method(setVideoOutputResolution)            \
    Method(setVolumeControlType)                \
    Method(toggleScreensaver)

namespace netflix {
namespace device {

class QADeviceBridge : public NfObject
{
    NFOBJECT_DECLARE(NfObject, NFOBJECT_NONE, QADevice_Methods);
public:
    QADeviceBridge(const std::shared_ptr<FileSystem> &sys);
    virtual ~QADeviceBridge();

    virtual Variant invoke(int method, const Variant &data);
    virtual Flags<NfObject::RequestFlag> requestFlags(const Request &request) const;

private:
    Variant getAuthenticationType(const Variant &data);
    Variant isSubtitleSupported(const Variant &data);
    Variant setSubtitleSupported(const Variant &data);
    Variant configurePreshared(const Variant &data);
    Variant configureWeb(const Variant &data);
    Variant configureModelGroupKey(const Variant &data);
    Variant getLanguage(const Variant &data);
    Variant setLanguage(const Variant &data);
    Variant getCapability(const Variant &data);
    Variant setCapability(const Variant &data);
    Variant getProfiles(const Variant &data);
    Variant setProfiles(const Variant &data);
    Variant sendCommand(const Variant &data);
    Variant setVideoOutputResolution(const Variant &data);
    Variant toggleScreensaver(const Variant &data);

    Variant setSupportedVideoOutput(const Variant &data);
    Variant setActiveVideoOutput(const Variant &data);
    Variant setVolumeControlType(const Variant &data);

    Variant getIpConnectivityMode(const Variant &data);
    Variant setIpConnectivityMode(const Variant &data);
    std::shared_ptr<FileSystem> mSystem;
};

}
}
#endif

#endif // QABRIDGE_H

