/*
 * (c) 1997-2016 Netflix, Inc.  All content herein is protected by
 * U.S. copyright and other applicable intellectual property laws and
 * may not be copied without the express permission of Netflix, Inc.,
 * which reserves all rights.  Reuse of any of this content for any
 * purpose without the permission of Netflix, Inc. is strictly
 * prohibited.
 */

#include "AgrippinaDeviceLib.h"

#include <cassert>
#include <fstream>

#include <nrdbase/Assert.h>
#include <nrdbase/Application.h>
#include <nrdbase/Log.h>
#include <nrdbase/ReadDir.h>
#include <nrddpi/WebCrypto.h>
#include <nrddpi/TeeApiCrypto.h>
#include <nrddpi/TeeApiStorage.h>
#include <nrddpi/TeeApiMgmt.h>


#include <nrd/AppLog.h>

#if defined(PLAYREADY3)
#include "PlayReady3DrmSystem.h"
#elif defined(PLAYREADY)
#include "PlayReadyDrmSystem.h"
#endif

#include "AgrippinaBufferManager.h"
#include "FileSystem.h"
#include "AgrippinaESManager.h"
#include "ManufSS.h"


using namespace std;
using namespace netflix;
using namespace netflix::device;


// Allocated in the extern function getDeviceLibOptions() and freed in
// ~AgrippinaDeviceLib(). Stores the reference app's settings for the ISystem
// capability parameters. Also stores a few reference-app-specific configuration
// parameters.
//
// Used to allow netflix-internal QA to run the netflix reference application
// with command-line options.  Device-specific implementations won't necessarily
// need this.
namespace netflix {
namespace device {
referenceDpiConfig* sConfiguration = 0;
}}

// This is the name of the attribute that identifies the checksum.
static string const CHECKSUM_ATTRIBUTE = "CHECKSUM";

#if !defined(PLAYREADY) && !defined(PLAYREADY3)
const std::string DrmSystem::type = "None";
#endif

// Helper function used when migrating older encrypted system data files to the
// newer encrypted format.
static void removeObsoleteKeys(map<string, string> &params)
{
    static const char *s_obsoleteKeys[] =
        {
            "MC/SDS",
            "MC/PDR",
            "MC/PFR",
            "MC/LNC",
            "Util/sF",
            NULL
        };

    map<string, string>::iterator it = params.begin();
    while (it != params.end())
    {
        bool found = false;
        for (size_t i=0; s_obsoleteKeys[i] != NULL; ++i)
        {
            if (it->first.find(string(s_obsoleteKeys[i])) != string::npos)
            {
                found = true;
                break;
            }
        }

        if (found)
            params.erase(it++);
        else
            ++it;
    }
}

// Earlier nrdapp versions used an unencrypted system data file.  If an old
// unencrypted data file exists on the device, the AgrippinaDeviceLib constructor
// uses this function to migrate it to the newer secure system data format.
static void migrateSystemData(const string &encryptedFile,
                              const string &nonEncryptedFile)
{
    // check for nonEncryptedFile
    ifstream is(nonEncryptedFile.c_str());
    if (!is.fail())
    {
        // migrate data from nonEncrypted to encryptedFile
        std::shared_ptr<ISystem> system(new FileSystem(encryptedFile));

        // read in existing encrypted pairs
        map<string, string> params(system->loadEncrypted());

        // read in nonencrypted pairs
        while (true)
        {
            string key, value;
            getline(is, key); if (!is) break;
            getline(is, value); if (!is) break;

            params[key] = value;
        }
        is.close();

        // erase any obsolete keys
        removeObsoleteKeys(params);

        // set checksum of new map to backdoor checksum
        params[CHECKSUM_ATTRIBUTE] = CHECKSUM_ATTRIBUTE;

        // store the map
        system->storeEncrypted(params);

        // remove the old non encrypted file
        remove(nonEncryptedFile.c_str());
    }
}


//
// The NrdLib creates a single instance of the device-specific implementation of
// IDeviceLib using the extern "C" call createDeviceLib()
// defined below. The createDeviceLib() call occurs after the
// getDeviceLibOptions() call that instantiates the sConfiguration stuct
// containing parameters referenced in the AgrippinaDeviceLib constructor.
bool AgrippinaDeviceLib::init()
{
    const string dataDir = Configuration::dataWritePath();
    NTRACE(TRACE_DPI, "\nNetflix reference DPI configuration\n"
           "Data directory path: %s\n\n"
           "%s",
           dataDir.c_str(),
           sConfiguration->toString().c_str());

    const string nonEncryptedFile = dataDir + "/nrd/systemData";
    const string encryptedFile = nonEncryptedFile + ".secure";
    string modelName = "Agrippina IntelCE4x00 OneBox";

    // Earlier versions used an uncrypted system data file.  If one exists, we
    // encrypt the data and move it to the newer secure system data file.
    migrateSystemData(encryptedFile, nonEncryptedFile);

    //If present, read idfile content
    DataBuffer fileContent;

#if 1
    if (sConfiguration->idFile.empty())
    {
        Log::fatal(TRACE_DPI, "No manufacturing secure store specified. Exiting.");
        Log::warn(TRACE_DPI, "You can generate the \"Manufacturing Secure Store\" file using the executable \"manufss\"");
        Log::warn(TRACE_DPI, "You can also launch netflix using the script \"netflix_manuss.sh\" which will call \"manufss\" before launching netflix");
        return false;
    }

    fileContent = DataBuffer::fromFile((sConfiguration->idFile).c_str());

    if (!fileContent.isBinary(-1))
    {
        Log::fatal(TRACE_DPI, "idfile configuration option must specify a \"Manufacturing Secure Store\" file, not IDFILE");
        Log::warn(TRACE_DPI, "You can generate the \"Manufacturing Secure Store\" file using the executable \"manufss\"");
        Log::warn(TRACE_DPI, "You can also launch netflix using the script \"netflix_manuss.sh\" which will call \"manufss\" before launching netflix");
        Log::fatal(TRACE_DPI, "Exiting the app");
        return false;
    }
#endif


    // allocate and initialize TEE and create a system object with TEE parameters
    teeCryptoPtr_.reset(new TeeApiCrypto());

    //find if TRACE_TEE is enabled and initialize TEE debug flags accordingly
    uint32_t teeDebugFlag = 0;
    if (app()) {
        teeDebugFlag = (app()->traceAreas()->isEnabled(TraceAreas::find("TEE"), Log::Trace)) ? 0xFFFFFFFF : 0x00000000;
    }

    teeMgmtPtr_.reset(new TeeApiMgmt(teeDebugFlag));

    NTRACE(TRACE_CRYPTO, "TEE: initializing");
    NFErr err = teeMgmtPtr_->initTee(fileContent);
    if (err != NFErr_OK){
        Log::error(TRACE_CRYPTO) << "Aborting since we could not initialize TEE. Error: " << err.toString();
        return false;
    }
    NTRACE(TRACE_CRYPTO, "successfully initialized TEE");

    // creating system object
    theSystem_.reset(new FileSystem(encryptedFile,
                                    sConfiguration->pubkeyFile,
                                    teeCryptoPtr_->getEsn(),
                                    modelName));
#if 1
    sConfiguration->mgk = true;
    theSystem_->setAuthenticationType(netflix::device::ISystem::MODEL_GROUP_KEYS);
#endif

    teeStoragePtr_.reset(new TeeApiStorage());
    err = teeStoragePtr_->init((uint32_t) theSystem_->getSecureStoreSize());
    if (err != NFErr_OK) {
        Log::fatal(TRACE_CRYPTO) << "Failed to initialize TEE API storage instance: " << err.toString();
        exit(0);
    }

    if (teeStoragePtr_)
        theSystem_->setTeeStorage(teeStoragePtr_);

    theBufferManager_.reset(new AgrippinaBufferManager(
                                sConfiguration->audioBufferPoolSize,
                                sConfiguration->videoBufferPoolSize));

    theElementaryStreamManager_.reset(new esplayer::AgrippinaESManager);

    // set esmanager
    theSystem_->AgrippinasetESManager(theElementaryStreamManager_);

#if defined(PLAYREADY)
    theDrmSystem_ = std::static_pointer_cast<IDrmSystem> ( std::shared_ptr<PlayReadyDrmSystem> (new PlayReadyDrmSystem) );
#elif defined(PLAYREADY3)
    theDrmSystem_ = std::static_pointer_cast<IDrmSystem> ( std::shared_ptr<PlayReady3DrmSystem> (new PlayReady3DrmSystem) );
#endif

    return true;
}


AgrippinaDeviceLib::~AgrippinaDeviceLib()
{

	theElementaryStreamManager_.reset();
    theDrmSystem_.reset();
    theBufferManager_.reset();
    theSystem_.reset();
    delete sConfiguration;
    sConfiguration = 0;

}

std::shared_ptr<IDrmSystem> AgrippinaDeviceLib::getDrmSystem()
{
    return theDrmSystem_;
}

std::shared_ptr<ISystem> AgrippinaDeviceLib::getSystem()
{
    return theSystem_;
}

std::shared_ptr<IWebCrypto> AgrippinaDeviceLib::getWebCrypto()
{
    // create on demand
    ScopedMutex lock(mutex_);
    if (!theWebCrypto_.get())
    {
        theWebCrypto_.reset(new WebCrypto(theSystem_->getAuthenticationType(), teeCryptoPtr_));
    }
    return theWebCrypto_;
}

std::shared_ptr<esplayer::IElementaryStreamManager> AgrippinaDeviceLib::getESManager()
{
    return theElementaryStreamManager_;
}

std::shared_ptr<IBufferManager> AgrippinaDeviceLib::getBufferManager()
{
    return theBufferManager_;
}

extern "C" {

//
// The NrdLib creates a single instance of the device-specific
// implementation of IDeviceLib using createDeviceLib()
// extern function defined here. A device-specific implementation should
// modify this so that it creates the device-specific implementation of
// IDeviceLib.
//
NRDP_EXPORTABLE IDeviceLib *createDeviceLib()
{
    AgrippinaDeviceLib *devicelib = new AgrippinaDeviceLib;
    if(!devicelib->init()) {
        delete devicelib;
        return 0;
    }
    return devicelib;
}
}

// Used by the netflix reference implementation so that the netflix app can
// be run with command-line parameters for netflix-internal QA of the
// netflix application.  Device-specific implementations other than the
// reference implementation do not need to define this extern function.

// Partners are not expected to ever need this functionality, so this should never be defined outside of Netflix's Reference DPI
ConfigurationOptions getDeviceLibOptions()
{
    if(!sConfiguration)
        sConfiguration = new referenceDpiConfig;
    //AgrippinaDeviceLib::sConfig = new AgrippinaDeviceLib::referenceDpiConfig();
    ConfigurationOptions options;
    options.push_back("Reference DPI Options");
#if 1
    options.push_back(ConfigurationOption('I', "idfile", ConfigurationOption::STRING_ARGUMENT, &sConfiguration->idFile, "Path to \"Manufacturing Secure Store\" file"));
#endif
    options.push_back(ConfigurationOption(0, "appboot-key", ConfigurationOption::DATAPATH_ARGUMENT, &sConfiguration->pubkeyFile, "Path to appboot public key file"));
    options.push_back(ConfigurationOption(0, "dpi-mgk",  &sConfiguration->mgk, "true if the ID file is for Model Group Key"));

    options.push_back(ConfigurationOption(0, "dpi-language", ConfigurationOption::STRING_ARGUMENT, &sConfiguration->language, "The language code the DPI will report."));
    options.push_back(ConfigurationOption(0, "dpi-friendlyname", ConfigurationOption::STRING_ARGUMENT, &sConfiguration->friendlyName, "The name that this device will advertise with for MDX and DIAL features."));

    options.push_back(ConfigurationOption(0, "dpi-has-pointer",  &sConfiguration->hasPointer, "true if the device has a pointing device connected"));
    options.push_back(ConfigurationOption(0, "dpi-has-keyboard",  &sConfiguration->hasKeyboard, "true if the device has a keyboard connected"));
    options.push_back(ConfigurationOption(0, "dpi-has-suspend",  &sConfiguration->hasSuspend, "true if the device supports suspend mode"));
    options.push_back(ConfigurationOption(0, "dpi-has-background",  &sConfiguration->hasBackground, "true if the device supports background mode"));
    options.push_back(ConfigurationOption(0, "dpi-support-2DVideoResize",  &sConfiguration->support2DVideoResize, "true if the device can resize the video window"));
    options.push_back(ConfigurationOption(0, "dpi-support-2DVideoResizeAnimation",  &sConfiguration->support2DVideoResizeAnimation, "true if the device can resize the video window during playback 5 times or more per second"));

    options.push_back(ConfigurationOption(0, "dpi-support-DrmPlaybackTransition",  &sConfiguration->supportDrmPlaybackTransition, "true if the device can make a seamless transition from non-DRMed playback to DRMed playback"));
    options.push_back(ConfigurationOption(0, "dpi-support-DrmToDrmTransition",  &sConfiguration->supportDrmToDrmTransition, "true if the device can make a seamless transition from one DRMed content to another DRMed content playback"));
    options.push_back(ConfigurationOption(0, "dpi-support-LimitedDurationLicense",  &sConfiguration->supportLimitedDurationLicenses, "true if the device can support limited duration license"));
    options.push_back(ConfigurationOption(0, "dpi-support-SecureStop",  &sConfiguration->supportSecureStop, "true if the drm subsystem can support secure stop"));
    options.push_back(ConfigurationOption(0, "dpi-support-DrmStorageDeletion",  &sConfiguration->supportDrmStorageDeletion, "true if the drm subsystem has drm storage deletion feature"));
    options.push_back(ConfigurationOption(0, "dpi-support-OnTheFlyAudioSwitch",  &sConfiguration->supportOnTheFlyAudioSwitch, "true if the device can make an on the fly transition to different audio track"));
    options.push_back(ConfigurationOption(0, "dpi-support-OnTheFlyAudioCodecSwitch",  &sConfiguration->supportOnTheFlyAudioCodecSwitch, "true if the device can make an on the fly transition to different audio codec"));
    options.push_back(ConfigurationOption(0, "dpi-support-OnTheFlyAudioChannelsChange",  &sConfiguration->supportOnTheFlyAudioChannelsChange, "true if the device can make an on the fly transition to different audio codec channels"));
    options.push_back(ConfigurationOption(0, "dpi-videobufferpoolsize", ConfigurationOption::SIZE_ARGUMENT, &sConfiguration->videoBufferPoolSize, "size in bytes of the pool of buffers used to receive video stream data"));
    options.push_back(ConfigurationOption(0, "dpi-audiobufferpoolsize", ConfigurationOption::SIZE_ARGUMENT, &sConfiguration->audioBufferPoolSize, "size in bytes of the pool of buffers used to receive audio stream data"));

    options.push_back(ConfigurationOption(0, "dpi-support-avc", &sConfiguration->avcEnabled, "enable AVC (h.264) profiles"));
    options.push_back(ConfigurationOption(0, "dpi-support-hevc", &sConfiguration->hevcEnabled, "enable HEVC (h.265) profiles"));
    options.push_back(ConfigurationOption(0, "dpi-support-vp9", &sConfiguration->vp9Enabled, "enable VP9 profiles"));
    options.push_back(ConfigurationOption(0, "dpi-support-dolby-vision", &sConfiguration->dolbyVisionEnabled, "enable Dolby Vision profiles"));
    options.push_back(ConfigurationOption(0, "dpi-support-hdr10", &sConfiguration->hdr10Enabled, "enable HDR10 profiles"));

    options.push_back(ConfigurationOption(0, "dpi-videoscreenwidth", ConfigurationOption::INT_ARGUMENT, &sConfiguration->videoRendererScreenWidth, "Video renderer screen width"));
    options.push_back(ConfigurationOption(0, "dpi-videoscreenheight", ConfigurationOption::INT_ARGUMENT, &sConfiguration->videoRendererScreenHeight, "Video renderer screen height"));

    options.push_back(ConfigurationOption(0, "dpi-minAudioPtsGap", ConfigurationOption::SIZE_ARGUMENT, &sConfiguration->minAudioPtsGap, "minimal audio pts gap that device can support"));

    options.push_back(ConfigurationOption(0, "enable-sso", &sConfiguration->enableSSO, "true if app supports SSO, false otherwise"));
    options.push_back(ConfigurationOption(0, "enable-signup", &sConfiguration->enableSignup, "true if app supports Signup, false otherwise"));
    options.push_back(ConfigurationOption(0, "dpi-pincode", ConfigurationOption::STRING_ARGUMENT, &sConfiguration->pinCode, "pin code passed from the platform"));
    options.push_back(ConfigurationOption(0, "dpi-token-url", ConfigurationOption::STRING_ARGUMENT, &sConfiguration->tokenUrl, "BOBO URL"));
    options.push_back(ConfigurationOption(0, "dpi-header-list", ConfigurationOption::STRING_ARGUMENT, &sConfiguration->headerList, "BOBO comma separated custom headers"));

#if defined(NRDP_HAS_IPV6)
    options.push_back(ConfigurationOption(0,
                                          "dpi-ip-connectivity-mode",
                                          ConfigurationOption::INT_ARGUMENT,
                                          &sConfiguration->ipConnectivityMode,
                                          "ip connectivity mode selection [4 for ipv4/ 6 for ipv6/ 10 for dualstack]"));
#endif

    options.push_back(ConfigurationOption(0, "dpi-video-file-dump", ConfigurationOption::STRING_ARGUMENT, &sConfiguration->videoFileDump, "'all' to dump all video to a file. 'gop' to dump only the last GOP to a file."));
    options.push_back(ConfigurationOption(0, "dpi-video-hevc-decoder", ConfigurationOption::STRING_ARGUMENT, &sConfiguration->hevcDecoder, "Use 'ffmpeg' or 'vanguard' decoder for HEVC content"));

    options.push_back(ConfigurationOption(0, "support-https-streaming",  &sConfiguration->supportHttpsStreaming, "true to enable https streaming (default enabled)"));

    return options;
}
