/*
 * (c) 1997-2016 Netflix, Inc.  All content herein is protected by
 * U.S. copyright and other applicable intellectual property laws and
 * may not be copied without the express permission of Netflix, Inc.,
 * which reserves all rights. Reuse of any of this content for any
 * purpose without the permission of Netflix, Inc. is strictly
 * prohibited.
 */

#ifndef AGRIPIPINA_DEVICELIB_H_
#define AGRIPIPINA_DEVICELIB_H_

/** @file AgrippinaDeviceLib.h - Reference implementation of the IDeviceLib
 * interface
 *
 * A device partner may use or modify this header and accompanying .cpp file as
 * needed.
 */

#include <nrdbase/tr1.h>
#include <nrdbase/Mutex.h>

#include <nrd/IDeviceLib.h>
#include <nrd/ISystem.h>
#include <nrd/IAudioMixerOutput.h>
#include <nrd/IDrmSystem.h>
#include <nrd/config.h>

#include "AgrippinaESManager.h"
#include <sstream>


#ifndef NRDP_HAS_ESPLAYER
# error "This DPI implements IElementaryStreamPlayer but NrdLib was "    \
    "built for use with IPlaybackDevice. You need to build NrdLib "     \
    "with ESPlayer support (--nrdlib-esplayer)"
#endif

namespace netflix {

class TeeApiCrypto;
class TeeApiMgmt;
class TeeApiStorage;

namespace device {


// Device-specific implementations other than the reference implementation will
// not necessarily need this structure. The referenceDpiConfig structure stores
// parameter values that the netflix reference implementation may read from the
// command-line to enable automated testing of the reference implementation by
// netflix-internal QA.
//
// The referenceDpiConfig structure stores the reference app's settings for the
// system capability parameters defined in the ISystem interface.  It also
// stores a few reference-app-specific configuration parameters. One instance of
// this structure is allocated by the reference app and the instance persists
// until the AgrippinaDeviceLib object is destroyed when the application exits. The
// structure is allocated in an extern c function
// AgrippinaDeviceLib.cpp:getDeviceLibOptions() and is available globally via the
// sConfiguration referenceDpiConfig* extern declared below.
//
// The referenceDpiConfig constructor sets the default values for the reference
// app's ISystem capabilities and reference-app-specific configuration
// parameters.
struct referenceDpiConfig {

#if defined(NRDP_HAS_IPV6)
    llong ipConnectivityMode;
#endif

    bool mgk; // true if the idFile is for Model Group Key

    std::string idFile; // Path to individualization file
    std::string pubkeyFile; // Path to appboot public key file

    // The device language code. The language code should be in BCP-47 format.
    //
    // Note: The legacy ISO-639-1 two character code or ISO-639-2 three
    //       character code are both legitimate BCP-47 code. See
    //       http://tools.ietf.org/rfc/bcp/bcp47.txt for more details.
    //
    std::string language;

    std::string videoFileDump;
    std::string hevcDecoder;

    std::string friendlyName; // The device friendly name (DLNA friendly name)

    bool hasPointer;  // "true" if the device has a pointing device connected

    bool hasKeyboard; // "true" if the device has a keyboard connected

    bool hasSuspend;  // "true" if the device supports suspend mode

    bool hasBackground;  // "true" if the device supports background mode

    bool hasScreensaver; // "true" if the device has a  screen saver

    bool supportDrmPlaybackTransition; // "true" if the device can play
                                       // non-DRMed content first and can make a
                                       // transition to play DRMed content
                                       // seamlessly

    bool supportDrmToDrmTransition;

    bool supportLimitedDurationLicenses;

    bool supportSecureStop;

    bool supportDrmStorageDeletion;

    llong tcpReceiveBufferSize; // tcp receive buffer in bytes for video/audio
                                  // downloading sockets. 0 means using default.

    llong videoBufferPoolSize;  // The size in bytes of the pool of buffers
                                  // used to receive video stream data.

    llong audioBufferPoolSize;  // The size in bytes of the pool of buffers used
                                  // to receive audio stream data.

    bool support2DVideoResize;    // "true" if the device can resize the
                                  // video window


    bool support2DVideoResizeAnimation; // "true" if the device can resize the
                                        // video window at least 5 times a second.

    bool supportOnTheFlyAudioSwitch;

    bool supportOnTheFlyAudioCodecSwitch;

    bool supportOnTheFlyAudioChannelsChange;

    bool supportHttpsStreaming;

    bool avcEnabled;        // Enable the AVC profiles

    bool vp9Enabled;        // Enable the VP9 profiles

    bool hevcEnabled;       // Enable the HEVC and HDR profiles

    bool dolbyVisionEnabled;

    bool hdr10Enabled;

    llong minAudioPtsGap;

    // These set the size of the video window in the reference
    // application. Change the values set in the constructor below to change the
    // video window size.
    llong videoRendererScreenWidth;
    llong videoRendererScreenHeight;

    bool enableSSO;     // "true" if app supports SSO, "false" otherwise
    bool enableSignup;  // "true" if app supports Signup, "false" otherwise
    std::string tokenUrl;
    std::string headerList;
    std::string pinCode;

    referenceDpiConfig() :
#if defined(NRDP_HAS_IPV6)
        // If not specified via command line, ipconnectivityMode should be selected by
        // 1. QAbridge
        // 2. streamingConfig
        //
        // This is priotized only when valid value is provided by command line option
        // default is 0 to enable this only when command line option is set
        ipConnectivityMode(0),
#endif
        mgk(true), idFile(""), pubkeyFile(""),
        language("en"),
        videoFileDump("none"),
#if defined(__APPLE__)
        hevcDecoder("ffmpeg"),
#else
        hevcDecoder("vanguard"),
#endif
        friendlyName("Friendly Netflix Device"),
        hasPointer(true), hasKeyboard(true),
        hasSuspend(true), hasBackground(true),
        hasScreensaver(false),
        supportDrmPlaybackTransition(true),
        supportDrmToDrmTransition(true),
        supportLimitedDurationLicenses(true),
        supportSecureStop(true),
        supportDrmStorageDeletion(true),
        tcpReceiveBufferSize(128 * 1024),
        videoBufferPoolSize(m_kVideoPoolSize),
        audioBufferPoolSize(m_kAudioPoolSize),
        support2DVideoResize(true),
        support2DVideoResizeAnimation(true),
        supportOnTheFlyAudioSwitch(true),
        supportOnTheFlyAudioCodecSwitch(true),
        supportOnTheFlyAudioChannelsChange(true),
        supportHttpsStreaming(true),
        avcEnabled(true),
        vp9Enabled(true),
#ifdef BUILD_VANGUARDHEVC_VIDEO_DECODER
        hevcEnabled(true),
        dolbyVisionEnabled(true),
        hdr10Enabled(true),
#else
        hevcEnabled(false),
        dolbyVisionEnabled(false),
        hdr10Enabled(false),
#endif
        minAudioPtsGap(0),
        videoRendererScreenWidth(1280),
        videoRendererScreenHeight(720),
        enableSSO(false),
        enableSignup(false),
        tokenUrl(""),
        headerList(""),
        pinCode("")
    { };

    std::string toString() const {
        std::stringstream data;
        data << "Individualization file: " << idFile
             << "\nAppBoot Public Key file: " << pubkeyFile
             << "\nLanguage: " << language
             << "\nFriendly Name: " << friendlyName
             << "\n\nHas Pointer: " << hasPointer
             << "\nHas Keyboard: " << hasKeyboard
             << "\nHas Suspend: " << hasSuspend
             << "\nHas Background: " << hasBackground
             << "\nSupports 2D video window resize: " << support2DVideoResize
             << "\nSupports 2D video window resize animation: " << support2DVideoResizeAnimation
             << "\nSupports non-DRM to DRM playback transition:" << supportDrmPlaybackTransition
             << "\nSupports DRM to DRM playback transition:" << supportDrmToDrmTransition
             << "\nVideo Buffer Pool Size: " << videoBufferPoolSize
             << "\nAudio Buffer Pool Size: " << audioBufferPoolSize
             << "\nVideo renderer screen size: " << videoRendererScreenWidth << "x" << videoRendererScreenHeight;

        return data.str();
    }

    // The size configuration for buffer pools.
    // Please refer to Netflix's portal for detailed requirements for
    // required and optimal audio and video buffer pool sizes.
    //
    // At the time of this comment, these requirements are listed here:
    // https://nrd.netflix.com/docs/development/nrdp43/device-porting-interface-dpi/content-playback/audio-and-video-buffer-sizes

    static const uint32_t m_kAudioPoolSize = 2252*1024*2;
    static const uint32_t m_kVideoPoolSize = 185*1024*1024;
};


// Used to allow netflix-internal QA to run the netflix reference application
// with command-line options. Device-specific implementations won't necessarily
// need this.
//
// In the netflix reference application, this extern points to an object that is
// allocated in extern c function AgrippinaDeviceLib.cpp:getDeviceLibOptions() and
// lives for the lifetime of the application.
//
extern referenceDpiConfig* sConfiguration;

class FileSystem;

class AgrippinaDeviceLib : public IDeviceLib
{
public:
    inline AgrippinaDeviceLib() : mutex_(DEVICELIB_MUTEX, "AgrippinaDeviceLib") { }
    bool init();

    virtual std::shared_ptr<IDrmSystem> getDrmSystem();
    virtual std::shared_ptr<ISystem> getSystem();
    virtual std::shared_ptr<IWebCrypto> getWebCrypto();
    virtual std::shared_ptr<esplayer::IElementaryStreamManager> getESManager();
    virtual std::shared_ptr<IBufferManager> getBufferManager();

    virtual ~AgrippinaDeviceLib();

private:
    Mutex mutex_;
    std::shared_ptr<IDrmSystem> theDrmSystem_;
    std::shared_ptr<FileSystem> theSystem_;
    std::shared_ptr<IWebCrypto> theWebCrypto_;
    std::shared_ptr<esplayer::AgrippinaESManager> theElementaryStreamManager_;
    std::shared_ptr<IBufferManager> theBufferManager_;
    std::shared_ptr<TeeApiCrypto> teeCryptoPtr_;
    std::shared_ptr<TeeApiStorage> teeStoragePtr_;
    std::shared_ptr<TeeApiMgmt> teeMgmtPtr_;
};

}}

#endif /* AGRIPIPINA_DEVICELIB_H_ */
