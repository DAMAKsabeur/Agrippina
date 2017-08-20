/*
 * (c) 1997-2016 Netflix, Inc.  All content herein is protected by
 * U.S. copyright and other applicable intellectual property laws and
 * may not be copied without the express permission of Netflix, Inc.,
 * which reserves all rights.  Reuse of any of this content for any
 * purpose without the permission of Netflix, Inc. is strictly
 * prohibited.
 */

#include "QADeviceBridge.h"
#ifdef NRDP_HAS_QA

#include <fstream>
#include <sstream>
#include <iostream>

#include <nrdbase/Base64.h>
#include <nrdbase/LexicalCast.h>
#include <nrdbase/StringTokenizer.h>

#include <nrd/ISystem.h>
#include <nrd/Request.h>

#include "AgrippinaDeviceLib.h"

using namespace std;
using namespace netflix;
using namespace netflix::device;

extern std::vector<unsigned char> ncf_kpe;
extern std::vector<unsigned char> ncf_kph;

#define READ_NBP_ARG(name)                              \
    std::string name;                                   \
    {                                                   \
        bool ok;                                        \
        name = data.mapValue<std::string>(#name, &ok);  \
        if (!ok)                                        \
            return Variant();                           \
    }

// ---------------------------------------------------------------------------

static void clearKeys()
{
    ncf_kpe.clear();
    ncf_kph.clear();
}

// ---------------------------------------------------------------------------

inline std::string authenticationTypeToString(device::ISystem::AuthenticationType t)
{
    switch(t)
    {
        case device::ISystem::PRESHARED_KEYS:   return "preshared_keys";
        case device::ISystem::WEB:              return "web";
        case device::ISystem::X509:             return "x509";
        case device::ISystem::ECC_LINK:         return "ecc_link";
        case device::ISystem::NPTICKET:         return "npticket";
        case device::ISystem::MODEL_GROUP_KEYS: return "model_group_keys";
        case device::ISystem::UNKNOWN:          return "unknown";
    }

    return "unknown";
}


inline ISystem::VideoOutputType stringToVideoOutputType(const std::string& p)
{

    if (p == "digital-internal")      return ISystem::DIGITAL_INTERNAL;
    if (p == "digital-miracast")      return ISystem::DIGITAL_MIRACAST;
    if (p == "digital-display-port")  return ISystem::DIGITAL_DISPLAY_PORT;
    if (p == "digital-dvi")           return ISystem::DIGITAL_DVI;
    if (p == "digital-hdmi")          return ISystem::DIGITAL_HDMI;
    if (p == "digital-external-other") return ISystem::DIGITAL_EXTERNAL_OTHER;
    if (p == "analog-composite")      return ISystem::ANALOG_COMPOSITE;
    if (p == "analog-component")      return ISystem::ANALOG_COMPONENT;
    if (p == "analog-vga")            return ISystem::ANALOG_VGA;
    if (p == "analog-scart")          return ISystem::ANALOG_SCART;
    if (p == "analog-other")          return ISystem::ANALOG_OTHER;
    return ISystem::ANALOG_OTHER; // not quite true, but should not happen since we write test script
}

inline ISystem::HDCPVersion stringToHDCPVersion(const std::string& p)
{
    if (p == "hdcp-not-applicable")   return ISystem::HDCP_NOT_APPLICABLE;
    if (p == "hdcp-not-engaged")      return ISystem::HDCP_NOT_ENGAGED;
    if (p == "hdcp-1-0")              return ISystem::HDCP_1_0;
    if (p == "hdcp-1-1")              return ISystem::HDCP_1_1;
    if (p == "hdcp-1-2")              return ISystem::HDCP_1_2;
    if (p == "hdcp-1-3")              return ISystem::HDCP_1_3;
    if (p == "hdcp-1-4")              return ISystem::HDCP_1_4;
    if (p == "hdcp-2-0")              return ISystem::HDCP_2_0;
    if (p == "hdcp-2-1")              return ISystem::HDCP_2_1;
    if (p == "hdcp-2-2")              return ISystem::HDCP_2_2;
    return ISystem::HDCP_NOT_APPLICABLE; // not quite true, but should not happen since we write test script
}

Variant QADeviceBridge::getAuthenticationType(const Variant &data)
{
    (void)data;
    ISystem::AuthenticationType auth = mSystem->getAuthenticationType();
    return Variant("authenticationType", authenticationTypeToString(auth));
}

#if 0
Variant QADeviceBridge::isSubtitleSupported(const Variant &data)
{
    READ_NBP_ARG(iso639_1);
    READ_NBP_ARG(iso639_2);
    READ_NBP_ARG(bcp47);
    bool result = mSystem->isSubtitleSupported(iso639_1, iso639_2, bcp47);

    return Variant("result", result);
}

Variant QADeviceBridge::setSubtitleSupported(const Variant &data)
{
    // set the value in the faked device
    READ_NBP_ARG(iso639_1);
    READ_NBP_ARG(iso639_2);
    READ_NBP_ARG(bcp47);

    QADeviceOverrides *qa = getOverrides();
    qa->setSubtitleSupported(iso639_1, iso639_2, bcp47);

    return Variant("result", true);
}
#endif

Variant QADeviceBridge::configurePreshared(const Variant &data)
{
    READ_NBP_ARG(esn);
    READ_NBP_ARG(kpe);
    READ_NBP_ARG(kph);

    clearKeys();

    mSystem->mEsn = esn;
    mSystem->mAuthType = ISystem::PRESHARED_KEYS;

    kpe = Base64::decode(kpe);
    kph = Base64::decode(kph);
    ncf_kpe.assign(kpe.begin(), kpe.end());
    ncf_kph.assign(kph.begin(), kph.end());

    return Variant("result", true);
}

Variant QADeviceBridge::configureModelGroupKey(const Variant &data)
{
    READ_NBP_ARG(esn);
    READ_NBP_ARG(kpe);
    READ_NBP_ARG(kph);

    clearKeys();

    mSystem->mEsn = esn;
    mSystem->mAuthType = ISystem::MODEL_GROUP_KEYS;

    kpe = Base64::decode(kpe);
    kph = Base64::decode(kph);
    ncf_kpe.assign(kpe.begin(), kpe.end());
    ncf_kph.assign(kph.begin(), kph.end());

    return Variant("result", true);
}

Variant QADeviceBridge::setLanguage(const Variant &data)
{
    READ_NBP_ARG(language);

    sConfiguration->language = language;
    mSystem->mEventDispatcher->languageChanged();

    return Variant("result", true);
}

Variant QADeviceBridge::setCapability(const Variant &data)
{
    READ_NBP_ARG(capability);
    READ_NBP_ARG(value);

    bool handled = false;

#define HANDLE_CAPABILITY(type, cap)                            \
    do {                                                        \
        if (capability == #cap) {                               \
            sConfiguration->cap = lexical_cast<type>(value);    \
            handled = true;                                     \
        }                                                       \
    } while(0)

    HANDLE_CAPABILITY(bool, hasPointer);
    HANDLE_CAPABILITY(bool, hasKeyboard);
    HANDLE_CAPABILITY(bool, hasSuspend);
    HANDLE_CAPABILITY(bool, hasBackground);
    HANDLE_CAPABILITY(bool, hasScreensaver);

    HANDLE_CAPABILITY(bool, support2DVideoResize);
    HANDLE_CAPABILITY(bool, support2DVideoResizeAnimation);

    HANDLE_CAPABILITY(int32_t, tcpReceiveBufferSize);

#undef HANDLE_CAPABILITY

    if (!handled)
        return Variant("error", "unknown capability");

    mSystem->mEventDispatcher->capabilityChanged();

    return Variant("result", true);
}

Variant QADeviceBridge::toggleScreensaver(const Variant &)
{
    mSystem->mScreensaverOn = !mSystem->mScreensaverOn;
    mSystem->mEventDispatcher->screensaverStateChanged();

    return Variant("result", true);
}

Variant QADeviceBridge::setSupportedVideoOutput(const Variant &data)
{
    mSystem->mSupportedVideoOutput.clear();

    // tokenize the arguments
    READ_NBP_ARG(videoOutputInfos);

    vector<string> videoOutputInfo;
    StringTokenizer::split(videoOutputInfos, videoOutputInfo, ";");
    for (vector<string>::iterator sit = videoOutputInfo.begin();sit != videoOutputInfo.end(); sit++){
        struct ISystem::VideoOutputInfo info;
        vector<string> tokens;
        StringTokenizer::split(*sit, tokens, ",");
        info.name = tokens[0];
        info.videoOutput = stringToVideoOutputType( tokens[1] );
        info.hdcpVersion = stringToHDCPVersion( tokens[2] );
        cout<< "pushing into mSupportedVideoOutput" << endl;

        mSystem->mSupportedVideoOutput.push_back(info);
    }
    // supportedVideo output is actually static value, which is not intended to be changed.
    return Variant("result", true);
}

Variant QADeviceBridge::setActiveVideoOutput(const Variant &data)
{
    mSystem->mActiveVideoOutput.clear();

   // tokenize the arguments
    READ_NBP_ARG(videoOutputStates);

    vector<string> videoOutputState;
    StringTokenizer::split(videoOutputStates, videoOutputState, ";");
    for (vector<string>::iterator sit = videoOutputState.begin();sit != videoOutputState.end(); sit++){
        struct ISystem::VideoOutputState state;
        vector<string> tokens;
        StringTokenizer::split(*sit, tokens, ",");
        state.videoOutput = stringToVideoOutputType( tokens[0] );
        state.hdcpVersion = stringToHDCPVersion( tokens[1] );
        cout << "pushing into mActiveVideoOutput" << endl;

        mSystem->mActiveVideoOutput.push_back(state);
    }
    mSystem->mEventDispatcher->videoOutputStatusChanged();

    return Variant("result", true);
}

Variant QADeviceBridge::setProfiles(const Variant &data)
{
    // tokenize the arguments
    READ_NBP_ARG(profiles);

    vector<string> avail;
    StringTokenizer::split(profiles, avail, ";");

    for (vector<string>::iterator av = avail.begin(); av != avail.end(); ++av) {
        vector<string> tokens;
        StringTokenizer::split(*av, tokens, ",");

        vector<ContentProfile> videos;
        vector<ContentProfile> audios;
        vector<ContentProfile> subtitles;

        // add args to appropriate vectors
        for (vector<string>::iterator it = tokens.begin(); it != tokens.end(); ++it) {
            ContentProfile profile = stringToProfile(*it);
            if (profile == UNKNOWN_PROFILE)
                continue;

            else if (profile < LAST_VIDEO_PROFILE)
                videos.push_back(profile);
            else if (profile < LAST_AUDIO_PROFILE)
                audios.push_back(profile);
            else
                subtitles.push_back(profile);
        }

        // set the new capability
        if ( videos.size() )
            mSystem->mVideoProfiles = videos;
        if ( audios.size() )
            mSystem->mAudioProfiles = audios;

    }

    mSystem->mEventDispatcher->capabilityChanged();

    return Variant("result", true);
}

Variant QADeviceBridge::sendCommand(const Variant &data)
{
    const std::string command = data.mapValue<std::string>("command");
    mSystem->mEventDispatcher->commandReceived(command);
    return Variant("result", true);
}

Variant QADeviceBridge::setVideoOutputResolution(const Variant &data)
{
    bool ok;
    const int width = data.mapValue<int>("width", &ok);
    if (!ok)
        return Variant("invalidArgumentError", "width");
    const int height = data.mapValue<int>("height", &ok);
    if (!ok)
        return Variant("invalidArgumentError", "height");

    sConfiguration->videoRendererScreenWidth = width;
    sConfiguration->videoRendererScreenHeight = height;

    mSystem->mEventDispatcher->videoOutputResolutionChanged();

    return Variant("result", true);
}

Variant QADeviceBridge::setVolumeControlType(const Variant &data)
{
    bool ok;
    const int volumeControlType = data.mapValue<int>("volumeControlType", &ok);
    if (!ok)
        return Variant("invalidArgumentError", "volumeControlType");

    mSystem->mVolumeControlType = static_cast<VolumeControlType> (volumeControlType);
    return Variant("result", true);
}

Variant QADeviceBridge::getIpConnectivityMode(
                                const Variant &data)
{
    (void)data;
    return Variant("ipConnectivityMode", mSystem->mQaBridgeIpConnectivityMode);

}

Variant QADeviceBridge::setIpConnectivityMode(
                                const Variant &data)
{
    bool ok;
    const int mode = data.mapValue<int>("ipConnectivityMode", &ok);
    if (!ok)
        return Variant("invalidArgumentError", "ipConnectivityMode");

    mSystem->mQaBridgeIpConnectivityMode = mode;
    mSystem->updateNetwork();

    return Variant("result", true);
}


#define READ_VAR(type, name)   var.mapValue<type>(name, &ok);

// ---------------------------------------------------------------------------

NFOBJECT_DEFINE(QADeviceBridge, NfObject, NFOBJECT_NONE, QADevice_Methods);
QADeviceBridge::QADeviceBridge(const std::shared_ptr<FileSystem> &system)
    : NfObject("device"), mSystem(system)
{
}

QADeviceBridge::~QADeviceBridge()
{
}

Flags<NfObject::RequestFlag> QADeviceBridge::requestFlags(const Request &request) const
{
    Flags<NfObject::RequestFlag> ret;
    switch (request.method()) {
    case Methods::getDolbyVisionRendering:
        ret |= Synchronous;
        break;
    default:
        break;
    }
    return ret;
}

Variant QADeviceBridge::invoke(int method, const Variant &data)
{
    Variant vm;

    switch (method) {

    case Methods::getAuthenticationType: {
        vm = getAuthenticationType(data);
        break; }
#if 0
    case Method::isSubtitleSupported: {
        vm = isSubtitleSupported(data);
        break; }
    case Method::setSubtitleSupported: {
        vm = setSubtitleSupported(data);
        break; }
#endif
    case Methods::configurePreshared: {
        vm = configurePreshared(data);
        break; }
#if 0
    case Methods::configureWeb: {
        vm = configureWeb(data);
        break; }
#endif
    case Methods::configureModelGroupKey: {
        vm = configureModelGroupKey(data);
        break; }
    case Methods::setLanguage: {
        vm = setLanguage(data);
        break; }
    case Methods::setCapability: {
        vm = setCapability(data);
        break; }
    case Methods::toggleScreensaver: {
        vm = toggleScreensaver(data);
        break; }
    case Methods::setProfiles: {
        vm = setProfiles(data);
        break; }
    case Methods::sendCommand: {
        vm = sendCommand(data);
        break; }
    case Methods::setVideoOutputResolution: {
        vm = setVideoOutputResolution(data);
        break; }
    case Methods::getIpConnectivityMode: {
        vm = getIpConnectivityMode( data );
        break; }
    case Methods::setIpConnectivityMode: {
        vm = setIpConnectivityMode( data );
        break; }
    case Methods::setSupportedVideoOutput: {
        vm = setSupportedVideoOutput( data );
        break; }
    case Methods::setActiveVideoOutput: {
        vm = setActiveVideoOutput( data );
        break; }
    case Methods::setVolumeControlType: {
        vm = setVolumeControlType( data );
        break; }
    default:
        return NfObject::invoke(method, data);
    }

    bool ok;
    const int idx = data.mapValue<int>("IDX", &ok);
    if (ok) {
        Variant ev;
        ev["IDX"] = idx;
        ev["data"] = vm;
        parent()->sendEvent(name(), ev);
    } else {
        parent()->sendEvent(name(), vm);
    }

    return true;
}
#endif
