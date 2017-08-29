/*
 * (c) 1997-2016 Netflix, Inc.  All content herein is protected by
 * U.S. copyright and other applicable intellectual property laws and
 * may not be copied without the express permission of Netflix, Inc.,
 * which reserves all rights.  Reuse of any of this content for any
 * purpose without the permission of Netflix, Inc. is strictly
 * prohibited.
 */
#include "FileSystem.h"

#include <errno.h>
#include <fcntl.h>
#include <fstream>
#include <iterator>
#include <libgen.h>
#include <locale>
#include <signal.h>
#include <sstream>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#define ZLIB_CONST
#include <zlib.h>

#ifdef __linux__
#include <link.h>
#include <sys/mman.h>
#endif

#include <nrdbase/Assert.h>
#include <nrdbase/Base64.h>
#include <nrdbase/OpenSSLLib.h>
#include <nrdbase/EncodedFile.h>
#include <nrdbase/ReadDir.h>

#include <nrd/AppLog.h>
#include <nrd/NrdApplication.h>
#include <nrd/NrdConfiguration.h>
#include <nrd/NrdpBridge.h>

#include <nrddpi/TeeApiStorage.h>

#include "AgrippinaDeviceLib.h"

#ifdef NRDP_HAS_QA
#include "QADeviceBridge.h"
#endif

#ifdef BUILD_SSO
#include "SingleSignOnBridge.h"
#endif

#ifdef BUILD_SIGNUP
#include "SignupBridge.h"
#endif

#ifdef BUILD_VOICE
#include "VoiceBridge.h"
#endif

#include "SystemValues.h"
#include "SignupBridge.h"

using namespace netflix;
using namespace netflix::device;

// The private keys we store in memory (not TEE compliant)
DataBuffer ncf_kpe;
DataBuffer ncf_kph;
DataBuffer ncf_kav;

// Signatures of in-process shared objects
std::set<std::string> mSharedObjects;

namespace
{

//#define MIGRATE_40_TO_41_SECURESTORE
#ifdef MIGRATE_40_TO_41_SECURESTORE
/* ***************************************************************************
 * NRDP4.0 -> 4.1 SecureStore migration sample codea
 *
 *  - Special handling may be needed IF secure store format is different in
 *    between NRDP4.0 and NRDP4.1.
 *  - In the case of this reference implementation (partner/dpi/reference),
 *    we introduced the following differences in between NRDP4.0 and 4.1
 *     0) The secure store contents are compatible between NRDP4.0 and NRDP4.1
 *     1) NRDP4.1 encrypts the secure store data, whereas NRDP4.0 does not
 *     2) NRDP4.1 does not store key-value pair if value is empty, whereas
 *        NRDP4.0 stores them.
 *
 *    For the above differences, here is what needs to be done
 *     0) Nothing
 *     1) The simple format conversion is needed
 *     2) Checksum value that is embedded in NRDP4.0 secure store must be
 *        re-calculated without key-value pair with empty value
 *
 *  < NOTE >
 *    This macro (MIGRATE_40_TO_41_SECURESTORE) only functions properly when
 *    needToMigrateSecureStore() is implemented properly. The migration code
 *    does not even run unless you modify needToMigrateSecureStore().
 *
 * ************************************************************************** */
#include <openssl/md5.h>

// This is the name of the attribute that identifies the checksum.
static const std::string CHECKSUM_ATTRIBUTE("CHECKSUM");

// Calculates the MD5 of a map, and returns a base-64 encoding of
// the digest.
static std::string calculateChecksum(const std::map<std::string, std::string> &m)
{
    MD5_CTX md5;
    MD5_Init(&md5);

    std::map<std::string, std::string>::const_iterator i;
    std::map<std::string, std::string>::const_iterator const end = m.end();
    for (i = m.begin(); i != end; ++i) {
        MD5_Update(&md5, i->first.data(), i->first.length());
        MD5_Update(&md5, i->second.data(), i->second.length());
    }

    unsigned char digest[MD5_DIGEST_LENGTH];
    MD5_Final(digest, &md5);
    std::string value;
    Base64::encode(digest, digest + sizeof digest, back_inserter(value));
    return value;
}

static std::map<std::string, std::string> loadNRDP40SecureStore(const char *secureStoreFile)
{
    std::map<std::string, std::string> pairs;
    std::ifstream is(secureStoreFile);
    do {
        std::string key, value;
        getline(is, key);
        if (!is)
            break;
        getline(is, value);
        if (!is)
            break;
        pairs[key] = value;
    } while (true);
    return pairs;
}

static std::map<std::string, std::string> convertSecureStore(const char *secureStoreFile)
{
    std::map<std::string, std::string> pairs = loadNRDP40SecureStore(secureStoreFile);

    // First, validate the checksum
    if (!pairs.empty()) {
        std::map<std::string, std::string>::iterator it = pairs.find(CHECKSUM_ATTRIBUTE);
        if (it != pairs.end()) {
            const std::string checksum = it->second;
            pairs.erase(it); // remove checksum when calculating
            if (checksum != calculateChecksum(pairs)) {
                Log::error(TRACE_DPI, "Invalid checksum, clearing secure store");
                pairs.clear(); // invalid checksum
            }
        } else {
            Log::error(TRACE_DPI, "No checksum found in secure store, clearing secure store");
            pairs.clear(); // no checksum
        }
    }

    // Next, remove empty key-value pairs as TEE reference code cannot handle them
    for (std::map<std::string, std::string>::iterator it = pairs.begin(); it != pairs.end(); ++it) {
        if (it->second.empty()) {
            pairs.erase(it);
        }
    }

    // Finally, calculate checksum
    std::string checksum(calculateChecksum(pairs));
    pairs[CHECKSUM_ATTRIBUTE] = checksum;

    return pairs;
}

static bool needToMigrateSecureStore()
{
    // This function must be modified to correctly determine if migration is needed or not.
    // And of course, migration only needs to happen ONCE when upgrading from NRDP4.0 to NRDP4.1
    // The migration code will be triggered only when true is returned
    return false;
}

#endif // MIGRATE_40_TO_41_SECURESTORE

/**
* Verifies the specified filenames are not empty and creates their
 * parent directories if they do not already exist.
 *
 * @param[in] encrypted encrypted filename.
 */
static void verifyFile(const std::string &fileName)
{
    ASSERT(!fileName.empty(), "No file specified.");
    char *fileNameCopy = strdup(fileName.c_str());
    char *dirName = dirname(fileNameCopy);
    if (strcmp(dirName, ".") != 0) {
        struct stat statBuf;
        if (stat(dirName, &statBuf) != 0 && !ReadDir::recursiveMakeDirectory(dirName, S_IRWXU)) {
            ASSERT(0, "Directory could not be created %d", errno);
        }
    }
    free(fileNameCopy);
}

} // namespace anonymous

bool FileSystem::sQaBridgeRenderDolbyVisionBL = true;
bool FileSystem::sQaBridgeRenderDolbyVisionEL = true;
std::string FileSystem::sQaBridgeDolbyFileDump = "";

FileSystem::FileSystem(const std::string &encryptedFile, const std::string &pubkeyFile, const std::string &esn, const std::string &model)
    : mIpConnectivity(IP_V4), mEsn(esn), mAuthType(UNKNOWN), mVolumeControlType(NO_VOLUME_CONTROL),
      // mQaBridgeHEVCProfileEnvelope(ISystem::HEVC_MAIN10_L51),
      // mQaBridgeAVCProfileEnvelope(ISystem::AVC_MAIN_L40),
      mEncryptedFile(encryptedFile), mScreensaverOn(false)
{
    if (!model.empty()) {
        mDeviceModel = model;
    } else {
        mDeviceModel = "Netflix Agrippina-IntelCE4x00-";
        Variant bundleVersion = Configuration::configDataValues(Configuration::ConfigDataBundleVersion);
        if (bundleVersion.isStringMap() &&
            bundleVersion.find("platformVersion")!=bundleVersion.stringMapEnd()){
            Variant platformVersion = bundleVersion["platformVersion"];
            if (platformVersion.isString()){
                NTRACE(TRACE_DPI, "FileSystem::FileSystem() adding platformVersion %s to the mDeviceModel", platformVersion.string().c_str());
                mDeviceModel += platformVersion.string().c_str();
            }
        }
    }

    // Verify storage files.
    verifyFile(mEncryptedFile);

    THREAD_MC_EVENT.setPriority(80);
    THREAD_MC_PUMPING.setPriority(80);

    // Base Profile is for mobile devices and software decoder
    // mVideoProfiles.push_back(VIDEO_PLAYREADY_H264_BPL30_DASH);

    if (sConfiguration->avcEnabled) {
        // Main profile is for all CE devices.
        mVideoProfiles.push_back(VIDEO_PLAYREADY_H264_MPL30_DASH);
        mVideoProfiles.push_back(VIDEO_PLAYREADY_H264_MPL31_DASH);
        mVideoProfiles.push_back(VIDEO_PLAYREADY_H264_MPL40_DASH);
        mVideoProfiles.push_back(VIDEO_PLAYREADY_H264_SHPL31_DASH);
        mVideoProfiles.push_back(VIDEO_PLAYREADY_H264_SHPL40_DASH);
        mVideoProfiles.push_back(VIDEO_NONE_H264_MPL30_DASH);
#ifdef REQUEST_HD_NON_DRM_PROFILES
        mVideoProfiles.push_back(VIDEO_NONE_H264_MPL31_DASH);
        mVideoProfiles.push_back(VIDEO_NONE_H264_MPL40_DASH);
#endif
    }

    if (sConfiguration->vp9Enabled) {
        // VP9 SDR profiles
        mVideoProfiles.push_back(VP9_PROFILE2_L30_DASH_CENC_PRK);
        mVideoProfiles.push_back(VP9_PROFILE2_L31_DASH_CENC_PRK);
        mVideoProfiles.push_back(VP9_PROFILE2_L40_DASH_CENC_PRK);
        mVideoProfiles.push_back(VP9_PROFILE2_L41_DASH_CENC_PRK);
        mVideoProfiles.push_back(VP9_PROFILE2_L50_DASH_CENC_PRK);
        mVideoProfiles.push_back(VP9_PROFILE2_L51_DASH_CENC_PRK);
    }

    if (sConfiguration->hevcEnabled) {
        // CE devices must not declare any HEVC L20 or L21 profiles
        mVideoProfiles.push_back(HEVC_MAIN_L30_DASH_CENC);
        mVideoProfiles.push_back(HEVC_MAIN_L31_DASH_CENC);
        mVideoProfiles.push_back(HEVC_MAIN_L40_DASH_CENC);
        mVideoProfiles.push_back(HEVC_MAIN_L41_DASH_CENC);
        mVideoProfiles.push_back(HEVC_MAIN_L50_DASH_CENC);
        mVideoProfiles.push_back(HEVC_MAIN_L51_DASH_CENC);

        mVideoProfiles.push_back(HEVC_MAIN10_L30_DASH_CENC);
        mVideoProfiles.push_back(HEVC_MAIN10_L31_DASH_CENC);
        mVideoProfiles.push_back(HEVC_MAIN10_L40_DASH_CENC);
        mVideoProfiles.push_back(HEVC_MAIN10_L41_DASH_CENC);
        mVideoProfiles.push_back(HEVC_MAIN10_L50_DASH_CENC);
        mVideoProfiles.push_back(HEVC_MAIN10_L51_DASH_CENC);
    }

    if (sConfiguration->dolbyVisionEnabled) {
        mVideoProfiles.push_back(HEVC_DV_MAIN10_L30_DASH_CENC);
        mVideoProfiles.push_back(HEVC_DV_MAIN10_L31_DASH_CENC);
        mVideoProfiles.push_back(HEVC_DV_MAIN10_L40_DASH_CENC);
        mVideoProfiles.push_back(HEVC_DV_MAIN10_L41_DASH_CENC);
        mVideoProfiles.push_back(HEVC_DV_MAIN10_L50_DASH_CENC);
        mVideoProfiles.push_back(HEVC_DV_MAIN10_L51_DASH_CENC);

        mVideoProfiles.push_back(HEVC_DV_MAIN10_PROFILE5_L30_DASH_CENC_PRK);
        mVideoProfiles.push_back(HEVC_DV_MAIN10_PROFILE5_L31_DASH_CENC_PRK);
        mVideoProfiles.push_back(HEVC_DV_MAIN10_PROFILE5_L40_DASH_CENC_PRK);
        mVideoProfiles.push_back(HEVC_DV_MAIN10_PROFILE5_L41_DASH_CENC_PRK);
        mVideoProfiles.push_back(HEVC_DV_MAIN10_PROFILE5_L50_DASH_CENC_PRK);
        mVideoProfiles.push_back(HEVC_DV_MAIN10_PROFILE5_L51_DASH_CENC_PRK);
    }

    if (sConfiguration->hdr10Enabled) {
        mVideoProfiles.push_back(HEVC_HDR_MAIN10_L30_DASH_CENC);
        mVideoProfiles.push_back(HEVC_HDR_MAIN10_L31_DASH_CENC);
        mVideoProfiles.push_back(HEVC_HDR_MAIN10_L40_DASH_CENC);
        mVideoProfiles.push_back(HEVC_HDR_MAIN10_L41_DASH_CENC);
        mVideoProfiles.push_back(HEVC_HDR_MAIN10_L50_DASH_CENC);
        mVideoProfiles.push_back(HEVC_HDR_MAIN10_L51_DASH_CENC);
    }

    mAudioProfiles.push_back(AUDIO_HEAAC_2_DASH);
    mAudioProfiles.push_back(AUDIO_DDPLUS_2_DASH);
    mAudioProfiles.push_back(AUDIO_DDPLUS_5_1_DASH);

    mAudioEaseProfiles.push_back(EASE_LINEAR);
    mAudioEaseProfiles.push_back(EASE_INCUBIC);
    mAudioEaseProfiles.push_back(EASE_OUTCUBIC);

    mAudioDecodeProfiles.push_back(AUDIO_HEAAC_2_DASH);
    mAudioDecodeProfiles.push_back(AUDIO_DDPLUS_2_DASH);
    mAudioDecodeProfiles.push_back(AUDIO_DDPLUS_5_1_DASH);

    mQaBridgeIpConnectivityMode = 0;

    // Open appboot public key data file.
    ASSERT(!pubkeyFile.empty(), "No appboot public key data file specified.");
    ncf_kav = Configuration::resourceContent(pubkeyFile).decode(DataBuffer::Encoding_Base64);
    if (ncf_kav.empty()) {
        ncf_kav = DataBuffer::fromFile(pubkeyFile.c_str()).decode(DataBuffer::Encoding_Base64);
    }
    NRDP_OBJECTCOUNT_DESCRIBE(ncf_kav, "DPI_NCF_KAV");
    ASSERT(!ncf_kav.isEmpty(), "Cannot open %s for reading.", pubkeyFile.c_str());

#if defined(BUILD_OPENGL_VIDEO_RENDERER)
    mSupport2DVideoTexture = true;
#else
    mSupport2DVideoTexture = false;
#endif

#if 1
#if defined(__linux__) && defined(__i386)
    mSupportTraceroute = (geteuid() == 0);
#else
    mSupportTraceroute = false;
#endif
#endif

    // In this example model, FileSystem need to have
    // mAgrippinaESManager to get remaining capability of pipeline
    mAgrippinaESManager.reset();
}

FileSystem::~FileSystem()
{
    // make sure the static DataBuffers are destroyed before ObjectCount::sMutex
    // from static destructors.
    ncf_kpe.clear();
    ncf_kph.clear();
    ncf_kav.clear();

}

void FileSystem::init(std::shared_ptr<EventDispatcher> eventDispatcher)
{
    mEventDispatcher = eventDispatcher;

    // System audio
    {
        INSTRUMENTATION_INTERVAL_SWITCHED_SCOPED("", "nrdlib.start.initDeviceLib.initSystemAudioManager");
        mSystemAudioManager.reset(new SystemAudioManager);
    }

    // Start network monitor
    {
        INSTRUMENTATION_INTERVAL_SWITCHED_SCOPED("", "nrdlib.start.initDeviceLib.initNetworkMonitor");
        assert(!mNwMon.get());
        mNwMon.reset(new NetworkMonitor(this));
        Application::instance()->startTimer(mNwMon);
        mNwMon->readNetwork();
    }

    // Start audio video output monitor
    {
        INSTRUMENTATION_INTERVAL_SWITCHED_SCOPED("", "nrdlib.start.initDeviceLib.initAudioVideoOutputMonitor");
        assert(!mAudioVideoOutputMonitor.get());
        mAudioVideoOutputMonitor.reset(new AudioVideoOutputMonitor(this));
        Application::instance()->startTimer(mAudioVideoOutputMonitor);
    }

    {
        INSTRUMENTATION_INTERVAL_SWITCHED_SCOPED("", "nrdlib.start.initDeviceLib.addBridges");
        std::shared_ptr<NrdpBridge> nrdpBridge = nrdApp()->nrdpBridge();

#ifdef NRDP_HAS_QA
        assert(nrdpBridge);
        assert(nrdpBridge->child("qa"));
        nrdpBridge->child("qa")->addChild(
            std::shared_ptr<NfObject>(new QADeviceBridge(std::static_pointer_cast<FileSystem>(shared_from_this()))));
#endif

#ifdef BUILD_VOICE
        nrdpBridge->addChild(std::shared_ptr<NfObject>(new VoiceBridge()));
#endif

        if (sConfiguration->enableSignup) {
            std::shared_ptr<SignupBridge> bridge(new SignupBridge());
            nrdpBridge->addChild(bridge);
        }
    }
}

void FileSystem::shutdown()
{
    mEventDispatcher.reset();
}

PresharedFileSystem::PresharedFileSystem(const std::string &idFile, const std::string &pubkeyFile, const std::string &encryptedFile)
    : FileSystem(encryptedFile, pubkeyFile)
{
    mAuthType = PRESHARED_KEYS;

    // Open individualization data file.
    ASSERT(!idFile.empty(), "No individualization data file specified.");
    std::ifstream idData(idFile.c_str());
    idData.imbue(std::locale::classic());
    ASSERT(idData, "Cannot open %s for reading.", idFile.c_str());

    // Load individualization data.
    std::string kpe, kph;
    idData >> mEsn;
    idData >> kpe;
    idData >> kph;

    std::string deviceModel;
    idData >> deviceModel;
    if (!deviceModel.empty()) {
        mDeviceModel = deviceModel;
    }

    ncf_kpe = Base64::decode(kpe);
    NRDP_OBJECTCOUNT_DESCRIBE(ncf_kpe, "DPI_NCF_KPE");
    ncf_kph = Base64::decode(kph);
    NRDP_OBJECTCOUNT_DESCRIBE(ncf_kph, "DPI_NCF_KPH");
}

ModelGroupFileSystem::ModelGroupFileSystem(const std::string &idFile, const std::string &pubkeyFile, const std::string &encryptedFile)
    : FileSystem(encryptedFile, pubkeyFile)
{
    // Model group keys work the same way as pre-shared, so we still
    // assign to ncf_kpe and ncf_kph. Only the advertised
    // authentication type is different.
    mAuthType = MODEL_GROUP_KEYS;

    // Open individualization data file.
    ASSERT(!idFile.empty(), "No individualization data file specified.");
    std::ifstream idData(idFile.c_str());
    idData.imbue(std::locale::classic());
    ASSERT(idData, "Cannot open %s for reading.", idFile.c_str());

    // Load individualization data.
    std::string kpe, kph;
    idData >> mEsn;
    idData >> kpe;
    idData >> kph;

    std::string deviceModel;
    idData >> deviceModel;
    if (!deviceModel.empty()) {
        mDeviceModel = deviceModel;
    }

    ncf_kpe = Base64::decode(kpe);
    ncf_kph = Base64::decode(kph);
    NRDP_OBJECTCOUNT_DESCRIBE(ncf_kpe, "DPI_NCF_KPE");
    NRDP_OBJECTCOUNT_DESCRIBE(ncf_kph, "DPI_NCF_KPH");
    Log::error(TRACE_DPI, "kpe = %s",kpe.c_str());
    Log::error(TRACE_DPI, "kph = %s",kph.c_str());
    Log::error(TRACE_DPI, "mEsn = %s",mEsn.c_str());
    //Log::error(TRACE_DPI, "ncf_kpe = %d",ncf_kpe.mData);
}

bool FileSystem::isScreensaverOn() const
{
    // FIXME: When we have a screen saver.
    return mScreensaverOn;
}

void FileSystem::setRegistered(bool registered)
{
    static std::string activatedFile = (Configuration::dataWritePath() + "/nrdapp/activated");

    if (registered) {
        int fd = creat(activatedFile.c_str(), 0644);
        if (fd != -1)
            close(fd);
    } else
        unlink(activatedFile.c_str());
}

void FileSystem::storeEncrypted(const std::map<std::string, std::string> &data)
{
    // clear out values in our RAM map so that the map doesn't grow with stale
    // values.
    mLastMap = data;
}

std::map<std::string, std::string> FileSystem::loadEncrypted()
{
#ifdef MIGRATE_40_TO_41_SECURESTORE
    // needToMigrateSecureStore() must be implemented properly
    if (needToMigrateSecureStore()) {
        mLastMap = convertSecureStore(mEncryptedFile.c_str());
        flushEncrypted();
    }
#endif
    std::map<std::string, std::string> data;
    EncodedFile file;
    if (file.open(mEncryptedFile, EncodedFile::Read)) {
        while (true) {
            if (teeStoragePtr_) {
                DataBuffer keyEncrypted, valueEncrypted, key, value;
                file >> keyEncrypted >> valueEncrypted;
                if ((NFErr_OK == teeStoragePtr_->loadProtected(keyEncrypted, key))
                    && (NFErr_OK == teeStoragePtr_->loadProtected(valueEncrypted, value))) {
                    NTRACE(TRACE_DPI, "FileSystem::loadEncrypted() using TEE key %s(%s), value %s(%s)", key.c_str(),
                        keyEncrypted.toHexString().c_str(), value.c_str(), valueEncrypted.toHexString().c_str());
                    data[key.toString()] = value.toString();
                } else {
                    data.clear();
                    break;
                }
            } else {
                std::string key;
                file >> key;
                file >> data[key];
                NTRACE(TRACE_DPI, "FileSystem::loadEncrypted() key %s, value %s", key.c_str(), data[key].c_str());
            }
            if (file.atEnd()) {
                break;
            } else if (file.hasError()) {
                data.clear();
                break;
            }
        }
    }
    return data;
}

const std::string FileSystem::getLanguage() const
{
    return sConfiguration->language;
}

/**
 * NOTE TO PARTNERS:
 *
 * If you are using FileSystem::getCapability() as a starting point for your
 * DPI implementation, please ensure that these values are correct for your
 * specific device.
 *
 * sConfiguration is defined in AgrippinaDeviceLib.cpp and its purpose is to
 * allow options to be overridden on the command line.  You may not need
 * this capability in your application, in which case the proper values for
 * the configuration parameters can just be hardcoded here.  In any case,
 * be aware that the default values taken from sConfiguration will probably
 * not be correct for your device, and must be changed.
 */
ISystem::Capability FileSystem::getCapability() const
{
    Capability capability;
    capability.hasPointer = sConfiguration->hasPointer;
    capability.hasKeyboard = sConfiguration->hasKeyboard;
    capability.hasSuspend = sConfiguration->hasSuspend;
    capability.hasBackground = sConfiguration->hasBackground;
    capability.hasScreensaver = sConfiguration->hasScreensaver;

    capability.tcpReceiveBufferSize = sConfiguration->tcpReceiveBufferSize;

    capability.videoBufferPoolSize = sConfiguration->videoBufferPoolSize;
    capability.audioBufferPoolSize = sConfiguration->audioBufferPoolSize;

    capability.support2DVideoResize = sConfiguration->support2DVideoResize;
    capability.support2DVideoResizeAnimation = sConfiguration->support2DVideoResizeAnimation;

    capability.supportDrmPlaybackTransition = sConfiguration->supportDrmPlaybackTransition;

    capability.supportDrmToDrmTransition = sConfiguration->supportDrmToDrmTransition;

    capability.supportLimitedDurationLicenses = sConfiguration->supportLimitedDurationLicenses;

    capability.supportSecureStop = sConfiguration->supportSecureStop;

    capability.supportDrmStorageDeletion = sConfiguration->supportDrmStorageDeletion;

    capability.videoProfiles = mVideoProfiles;
    capability.audioProfiles = mAudioProfiles;
    capability.audioDecodeProfiles = mAudioDecodeProfiles;
    capability.audioEaseProfiles = mAudioEaseProfiles;

    capability.supportOnTheFlyAudioSwitch = sConfiguration->supportOnTheFlyAudioSwitch;
    capability.supportOnTheFlyAudioCodecSwitch = sConfiguration->supportOnTheFlyAudioCodecSwitch;
    capability.supportOnTheFlyAudioChannelsChange = sConfiguration->supportOnTheFlyAudioChannelsChange;

    capability.supportHttpsStreaming = sConfiguration->supportHttpsStreaming;

    capability.minAudioPtsGap = sConfiguration->minAudioPtsGap;

    return capability;
}

uint32_t FileSystem::getCertificationVersion() const
{
    return 0;
}

const std::string FileSystem::getEsn() const
{
    return mEsn;
}

void FileSystem::setEsn(std::string esn)
{
    mEsn = esn;
}

bool FileSystem::getDeviceModel(std::string &deviceModel) const
{
    deviceModel = mDeviceModel;
    NTRACE(TRACE_DPI, "FillSystem::getDeviceModel() %s", deviceModel.c_str());
    return true;
}

const std::string FileSystem::getFriendlyName() const
{
    return sConfiguration->friendlyName;
}

const std::string FileSystem::getSoftwareVersion() const
{
    static const std::string version("Netflix Agrippina-1.0 Sagemcom SAS");
    return version;
}

ISystem::AuthenticationType FileSystem::getAuthenticationType() const
{
    return mAuthType;
}

void FileSystem::setAuthenticationType(ISystem::AuthenticationType authType)
{
    mAuthType = authType;
}

void FileSystem::getVideoOutputResolution(uint32_t &width, uint32_t &height) const
{
    width = sConfiguration->videoRendererScreenWidth;
    height = sConfiguration->videoRendererScreenHeight;
}

void FileSystem::notifyApplicationState(netflix::device::ISystem::ApplicationState)
{
}

std::map<std::string, std::string> FileSystem::getStartupLogTags()
{
    return std::map<std::string, std::string>();
}

void FileSystem::reseedSslEntropy()
{
    int retVal;

    unsigned char entropyData[OpenSSLLib::MIN_SEED_LEN];

    if (FILE *entropyFile = fopen("/dev/urandom", "r")) {
        retVal = fread(entropyData, 1, OpenSSLLib::MIN_SEED_LEN, entropyFile);
        if (retVal > 0)
            OpenSSLLib::add_entropy(entropyData, retVal, 0.0);
        fclose(entropyFile);
    }

    return;
}

/** Get the platform preferred IP connectivity mode. */
IpConnectivityMode FileSystem::getIpConnectivityMode()
{
#ifdef NRDP_HAS_IPV6

    // return IP_V6;

    // To test app behavior from app launch, command line has priority
    if (sConfiguration->ipConnectivityMode == 4) {
        return IP_V4;
    } else if (sConfiguration->ipConnectivityMode == 6) {
        return IP_V6;
    } else if (sConfiguration->ipConnectivityMode == 10) {
        return IP_DUAL;
    }

    // if sConfiguration->ipConnectivityMode is not specified by command line
    // (value 0), honor QABridge value
    if (mQaBridgeIpConnectivityMode == 4) {
        return IP_V4;
    } else if (mQaBridgeIpConnectivityMode == 6) {
        return IP_V6;
    } else if (mQaBridgeIpConnectivityMode == 10) {
        return IP_DUAL;
    } else {
        return mIpConnectivity;
    }

#else
    return IP_V4;
#endif
}

std::vector<std::string> FileSystem::getDNSList()
{
    std::vector<std::string> dnslist;
    FILE *f = fopen("/etc/resolv.conf", "r");
    if (f) {
        char line[1024];
        while ((fgets(line, sizeof(line), f))) {
            if (!strncmp("nameserver", line, 10)) {
                char *ch = line + 10;
                if (isspace(*ch++)) {
                    while (isspace(*ch))
                        ++ch;
                    if (*ch) {
                        char *start = ch;
                        while (!isspace(*ch))
                            ++ch;
                        dnslist.push_back(std::string(start, ch - start));
                    }
                }
            }
        }
        fclose(f);
    }
    return dnslist;
}

#ifdef NRDP_HAS_TRACING
static std::string networkInterfacesToString(const std::vector<FileSystem::NetworkInterface> &interfaces)
{
    std::ostringstream str;
    for (std::vector<FileSystem::NetworkInterface>::const_iterator it = interfaces.begin(); it != interfaces.end(); ++it) {
        if (it != interfaces.begin())
            str << '\n';
        str << "Name: " << it->name << " PType: " << it->physicalLayerType << " PSubType: " << it->physicalLayerSubtype
            << " IP: " << it->ipAddress << " IPV6: " << StringTokenizer::join(it->ipv6Addresses, ", ") << " HWaddr: " << it->macAddress
            << " SSID: " << it->ssid << " WifiChannel: " << it->wirelessChannel << " Carrier: " << it->mobileCarrier << " ("
            << it->mobileCountryCode << ", " << it->mobileNetworkCode << ")"
            << " Link: " << it->linkConnected << " Internet: " << it->internetConnected;
        if (!it->additionalInfo.empty())
            str << " Additional: " << it->additionalInfo;
        if (it->isDefault)
            str << " (Default)";
    }
    return str.str();
}
#endif

bool FileSystem::NetworkMonitor::readNetwork()
{
    char *str1;
    unsigned int ipmap = 0x0; // bit 0 = valid ipv4  bit 1 = valid ipv6

    std::vector<NetworkInterface> *pnif = &(this->m_nif);
    std::vector<NetworkInterface> nif_test;
    nif_test = m_sys->getNetworkInterfaces();
    if (pnif->empty())
        *pnif = nif_test;

    // Check IP connectivity flag see if there is any change
    for (std::vector<ISystem::NetworkInterface>::iterator i = nif_test.begin(); i != nif_test.end(); i++) {
        if (i->isDefault) {
            char tmp[256];
            memset(tmp, 0, 256);
            if (!i->ipAddress.empty()) {
                strcpy(tmp, i->ipAddress.c_str());
                str1 = strtok(tmp, ".");
                if (!strcmp(str1, "127") || !strcmp(str1, "169"))
                    ipmap = ipmap | 0x00;
                else
                    ipmap = ipmap | 0x01;
            } else // empty IP, mark as invalid Ipv4
                ipmap = ipmap | 0x00;
            std::vector<std::string> v = i->ipv6Addresses;
            for (std::vector<std::string>::iterator x = v.begin(); x != v.end(); x++) {
                memset(tmp, 0, 256);
                strcpy(tmp, x->c_str());
                str1 = strtok(tmp, ":");
                if (!strcmp(str1, "") || !strcmp(str1, "fe80"))
                    ipmap = ipmap | 0x00;
                else
                    ipmap = ipmap | 0x02;
            }
        }
    }
    // Process the ipconnectivity flags
    // NTRACE(TRACE_DPI, "\n ==Connectivity mode: 0x%x == \n", ipmap );
    if (ipmap == 0x01)
        m_sys->mIpConnectivity = IP_V4; // ipv4
    else if (ipmap == 0x02)
        m_sys->mIpConnectivity = IP_V6; // ipv6
    else if (ipmap == 0x03)
        m_sys->mIpConnectivity = IP_DUAL; // dual mode
    else
        m_sys->mIpConnectivity = IP_DUAL; // default

    // see if there was a change in the DNS server list
    std::vector<std::string> dnslist = m_sys->getDNSList();
    bool update = false;
    if (dnslist.size() != mDnsList.size())
        update = true;
    if (!update) {
        // lists are the same size but may have different entries
        // Using an n squared algorithm assuming a small number
        // of DNS servers.
        std::vector<std::string>::iterator i;
        for (i = dnslist.begin(); i != dnslist.end(); i++) {
            std::vector<std::string>::iterator j;
            for (j = mDnsList.begin(); j != mDnsList.end(); j++) {
                if ((*i).compare((*j)) == 0)
                    break; // found it!
            }
            if (j == mDnsList.end()) {
                // we didn't find this entry in mDnsList,
                // lists are different
                update = true;
                break;
            }
        }
    }
    if (update) {
        NTRACE(TRACE_DPI, "DNS list changed");
    }

    size_t size = pnif->size();
    for (size_t i = 0; !update && i < size && i < nif_test.size(); i++) {
        //          NTRACE(TRACE_DPI, "\n\n\n [IF %s %s]\n\n",nif_test[i].ipAddress.c_str(),(*pnif)[i].ipAddress.c_str());
        if ((*pnif)[i].name != nif_test[i].name) {
            NTRACE(TRACE_DPI, "\n [IF name changed]");
            update = true;
        } else if ((*pnif)[i].ipAddress != nif_test[i].ipAddress) {
            NTRACE(TRACE_DPI, "\n [IF IP changed] interface %s", nif_test[i].name.c_str());
            update = true;
        } else if ((*pnif)[i].ipv6Addresses != nif_test[i].ipv6Addresses) {
            NTRACE(TRACE_DPI, "\n [IF IPv6 changed] interface %s", nif_test[i].name.c_str());
            update = true;
        } else if ((*pnif)[i].ssid != nif_test[i].ssid) {
            NTRACE(TRACE_DPI, "\n [IF SSID changed] interface %s", nif_test[i].name.c_str());
            update = true;
        } else if ((*pnif)[i].wirelessChannel != nif_test[i].wirelessChannel) {
            NTRACE(TRACE_DPI, "\n [IF WIRELESS channel changed] interface %s", nif_test[i].name.c_str());
            update = true;
        } else if ((*pnif)[i].macAddress != nif_test[i].macAddress) {
            NTRACE(TRACE_DPI, "\n [IF MAC changed] interface %s", nif_test[i].name.c_str());
            update = true;
        } else if ((*pnif)[i].isDefault != nif_test[i].isDefault) {
            NTRACE(TRACE_DPI, "\n [IF Default changed] interface %s", nif_test[i].name.c_str());
            update = true;
        } else if ((*pnif)[i].linkConnected != nif_test[i].linkConnected) {
            NTRACE(TRACE_DPI, "\n [IF PHY Connect changed] interface %s", nif_test[i].name.c_str());
            update = true;
        } else if ((*pnif)[i].internetConnected != nif_test[i].internetConnected) {
            NTRACE(TRACE_DPI, "\n [IF Internet Connect changed] interface %s", nif_test[i].name.c_str());
            update = true;
        } else {
            // NTRACE(TRACE_DPI, "\n [IF NO changed]");
        }
    }
    if (update) {
        NTRACE(TRACE_DPI, "Network changed DNS:\n%s\n=>\n%s\nIFace:\n%s\n=>\n%s", StringTokenizer::join(mDnsList, ", ").c_str(),
            StringTokenizer::join(dnslist, ", ").c_str(), networkInterfacesToString(*pnif).c_str(),
            networkInterfacesToString(nif_test).c_str());

        *pnif = nif_test;
        mDnsList = dnslist;
    }
    return update;
}

void FileSystem::NetworkMonitor::timerFired()
{
    if (readNetwork())
        m_sys->updateNetwork();
}

/**
 * The reference app only supports 2D view mode.
 */
ViewModeType FileSystem::getCurrentViewMode() const
{
    return UNKNOWN_VIEWMODE_TYPE;
}

/**
 * The reference app only supports 2D view mode.
 */
std::vector<ViewModeType> FileSystem::getAvailableViewModes() const
{
    std::vector<ViewModeType> types;
    types.push_back(UNKNOWN_VIEWMODE_TYPE);
    return types;
}

/**
 * The reference app only supports 2D view mode.
 */
void FileSystem::setViewMode(ViewModeType &viewMode)
{
    NRDP_UNUSED(viewMode);
}

VolumeControlType FileSystem::getVolumeControlType()
{
    return mSystemAudioManager->getVolumeControlType();
}

double FileSystem::getVolume()
{
    return mSystemAudioManager->getVolume();
}

void FileSystem::setVolume(double volume)
{
    mSystemAudioManager->setVolume(volume);
    mEventDispatcher->outputVolumeChanged();
}

double FileSystem::getCurrentMinimumVolumeStep()
{
    return mSystemAudioManager->getCurrentMinimumVolumeStep();
}

bool FileSystem::isMuted()
{
    return mSystemAudioManager->isMuted();
}

void FileSystem::setMute(bool muteSetting)
{
    mSystemAudioManager->setMute(muteSetting);
    mEventDispatcher->outputVolumeChanged();
}

void FileSystem::flushEncrypted()
{
    EncodedFile file;
    if (!file.open(mEncryptedFile, EncodedFile::Write))
        return;
    for (std::map<std::string, std::string>::const_iterator it = mLastMap.begin(); file.isValid() && it != mLastMap.end(); ++it) {
        NTRACE(TRACE_DPI, "FileSystem::flushEncrypted() [%s][%s]\n", it->first.c_str(), it->second.c_str());

        if (teeStoragePtr_) {
            // encrypt the data first
            const DataBuffer keyClr(it->first);
            DataBuffer key;
            const DataBuffer valueClr(it->second);
            DataBuffer value;
            if ((NFErr_OK == teeStoragePtr_->storeProtected(keyClr, key))
                && (NFErr_OK == teeStoragePtr_->storeProtected(valueClr, value))) {
                NVERBOSE(TRACE_DPI, "Encrypted: [%s][%s]", key.toHexString().c_str(), value.toHexString().c_str());
                file << key << value;
            }
        } else {
            file << it->first << it->second;
        }
    }
}

/**
 * Device Display Status
 */
std::vector<struct ISystem::VideoOutputInfo> FileSystem::getSupportedVideoOutput()
{
    if (mSupportedVideoOutput.size() == 0) {
        std::vector<ISystem::VideoOutputState> dummy;
        getAudioVideoOutputs(mSupportedVideoOutput, dummy);
    }
    return mSupportedVideoOutput;
}

std::vector<struct ISystem::VideoOutputState> FileSystem::getActiveVideoOutput()
{
    std::vector<struct ISystem::VideoOutputInfo> dummy;
    getAudioVideoOutputs(dummy, mActiveVideoOutput);
    NRDP_UNUSED(dummy);
    return mActiveVideoOutput;
}

#ifdef __linux__
static int dl_iterate_phdr_callback(struct dl_phdr_info *info, size_t size, void *data)
{
    (void)size;
    (void)data;

    if (info->dlpi_name[0]) {
        mSharedObjects.insert(info->dlpi_name);
    }
    return 0;
}
#endif

Variant FileSystem::getSignatures()
{
    Variant map;

#ifdef __linux__

    Dl_info info;
    dladdr((const void *)&dl_iterate_phdr_callback, &info);

    mSharedObjects.insert(info.dli_fname);

    dl_iterate_phdr(dl_iterate_phdr_callback, NULL);

    for (std::set<std::string>::iterator it = mSharedObjects.begin(); it != mSharedObjects.end(); ++it) {
        struct stat buf;
        buf.st_size = 0xDEADBEEF;
        uint32_t crc = 0xDEADBEEF;

        if (!stat(it->c_str(), &buf)) {
            int fd;
            if ((fd = open(it->c_str(), O_RDONLY)) != -1) {
                Bytef *p = (Bytef *)mmap(NULL, buf.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
                crc = crc32(0L, Z_NULL, 0);
                crc = crc32(crc, p, buf.st_size);
                munmap(p, buf.st_size);
                close(fd);
            }
        }

        Variant propmap;
        propmap["size"] = buf.st_size;
        propmap["crc32"] = crc;

        map[it->c_str()] = propmap;
    }
#endif

    return map;
}

void FileSystem::setTeeStorage(std::shared_ptr<TeeApiStorage> teeStoragePtr)
{
    teeStoragePtr_ = teeStoragePtr;
}

#ifdef NRDP_APPLICATION_MANAGER
extern void ApplicationManagerRequestSuspend(void);
extern void ApplicationManagerRequestBackground(void);
#endif

void FileSystem::requestSuspend(const std::string &reason)
{
#ifdef NRDP_APPLICATION_MANAGER
    // Partners should hook here when the UI requests to move into either background
    // or suspend state.  If the partner application manager doesn't support background
    // mode then they should use suspend mode.  This example will ignore the "reason"
    // and always request the application manager suspend Netflix.

    if (reason == "background") {
        ApplicationManagerRequestBackground();
    } else {
        ApplicationManagerRequestSuspend();
    }
#else
    nrdApp()->setSuspended(true, reason);
#endif
}

Variant FileSystem::getSystemValue(const std::string &key)
{
    return SystemValues::getSystemValue(key);
}

Variant FileSystem::getReport()
{
    Variant report;
    std::string reason;
    std::string dump;

    const char *reportFile = "/tmp/netflix_report";

    do {
        std::string tmp;

        std::ifstream is(reportFile);
        getline(is, reason);
        if (!is)
            break;
        getline(is, dump);
        if (!is)
            break;

        do {
            getline(is, tmp);
            if (!is)
                break;
            dump.append("\n");
            dump.append(tmp);
        } while (1);

        report["crashCode"] = SIGKILL;
        report["reason"] = reason;
        report["dump"] = dump;
    } while (0);

    unlink(reportFile);

    return report;
}

void FileSystem::getUptime(uint32_t &uptime, uint32_t &ontime)
{
    // Reference x86 implementation doesn't have a real application manager
    // so the best we can do is use Linux methods and use the power manager
    // messages in the kernel log.

    const char *powerFile = "/sys/power/wakeup_count";
    const char *uptimeFile = "/proc/uptime";
    const char *kernlogFile = "/var/log/kern.log";
    const char *wakemsg = "PM: Finishing wakeup.";
    const char *kernmsg = "kernel: [";

    std::string tmp;

    std::ifstream us(uptimeFile);
    getline(us, tmp);
    uptime = atoi(tmp.c_str());

    std::ifstream is(powerFile);
    getline(is, tmp);

    if (tmp == "0") {
        // Device has never slept so ontime is Linux uptime
        ontime = uptime;
    } else {
        // Device has slept so find last PM wakeup message
        uint32_t eventTime = 0;
        std::ifstream file(kernlogFile);
        std::string str;
        while (std::getline(file, str)) {
            const char *a = strstr(str.c_str(), wakemsg);
            if (!a)
                continue;
            a = strstr(str.c_str(), kernmsg);
            if (!a)
                continue;
            a += strlen(kernmsg);
            eventTime = atoi(a);
        }
        ontime = uptime - eventTime;
    }
}

void FileSystem::AgrippinasetESManager(std::shared_ptr<esplayer::AgrippinaESManager> ESManager)
{
    mAgrippinaESManager = ESManager;
}

void FileSystem::updateNetwork()
{
    mEventDispatcher->networkChanged();
}


/**************************************************************
        Audio video interface monitor implementation
***************************************************************/
bool FileSystem::AudioVideoOutputMonitor::readAudioVideoOutput()
{
    std::vector<struct VideoOutputInfo> currentSupportedVideoOutputs;
    std::vector<struct VideoOutputState> currentActiveVideoOutputs;

    m_sys->getAudioVideoOutputs(currentSupportedVideoOutputs, currentActiveVideoOutputs);

    bool changed = false;

    if (lastActiveVideoOutputs.size() != currentActiveVideoOutputs.size()) {
        changed = true;
    } else {
        for (uint32_t i = 0; i < currentActiveVideoOutputs.size(); ++i) {
            if ((lastActiveVideoOutputs[i].videoOutput != currentActiveVideoOutputs[i].videoOutput) ||
                (lastActiveVideoOutputs[i].hdcpVersion != currentActiveVideoOutputs[i].hdcpVersion) ||
                (lastActiveVideoOutputs[i].edid != currentActiveVideoOutputs[i].edid) ||
                (lastActiveVideoOutputs[i].outputWidthInPixels != currentActiveVideoOutputs[i].outputWidthInPixels) ||
                (lastActiveVideoOutputs[i].outputHeightInPixels != currentActiveVideoOutputs[i].outputHeightInPixels) ||
                (lastActiveVideoOutputs[i].displayWidthInPixels != currentActiveVideoOutputs[i].displayWidthInPixels) ||
                (lastActiveVideoOutputs[i].displayHeightInPixels != currentActiveVideoOutputs[i].displayHeightInPixels) ||
                (lastActiveVideoOutputs[i].displayWidthInCentimeters != currentActiveVideoOutputs[i].displayWidthInCentimeters) ||
                (lastActiveVideoOutputs[i].displayHeightInCentimeters != currentActiveVideoOutputs[i].displayHeightInCentimeters)) {
                    changed = true;
                    break;
            }
        }
    }

    if (changed) {
        lastSupportedVideoOutputs = currentSupportedVideoOutputs;
        lastActiveVideoOutputs = currentActiveVideoOutputs;
    }

    return changed;
}

void FileSystem::AudioVideoOutputMonitor::timerFired()
{
    if (readAudioVideoOutput()) {
        m_sys->updateAudioVideoOutput();
    }
}

void FileSystem::updateAudioVideoOutput()
{
    mEventDispatcher->videoOutputStatusChanged();
}

void FileSystem::getAudioVideoOutputs(
            std::vector<struct VideoOutputInfo> & videoOutputs,
            std::vector<struct VideoOutputState> & activeVideoOutputs)
{
#if 0
    videoOutputs.clear();
    activeVideoOutputs.clear();

#ifdef __linux__
    std::string cardPath("/sys/class/drm/card0/");
    struct stat buf;
    struct dirent *dir;
    DIR * d = opendir(cardPath.c_str());
    if (d) {
        while ((dir = readdir(d)) != NULL) {
            if (strcmp(dir->d_name,".") && strcmp(dir->d_name,"..")) {
                std::string enabledPath = cardPath + dir->d_name + "/enabled";
                std::string statusPath = cardPath + dir->d_name + "/status";
                std::string edidPath = cardPath + dir->d_name + "/edid";

                if (!stat(enabledPath.c_str(), &buf)) {
                    VideoOutputInfo videoOutputInfo;

                    if (strstr(dir->d_name, "HDMI")) {
                        videoOutputInfo.videoOutput = DIGITAL_HDMI;
                        videoOutputInfo.hdcpVersion = HDCP_2_2;
                    } else if (strstr(dir->d_name, "DVI")) {
                        videoOutputInfo.videoOutput = DIGITAL_DVI;
                        videoOutputInfo.hdcpVersion = HDCP_2_2;
                    } else if (strstr(dir->d_name, "DISPLAYPORT")) {
                        videoOutputInfo.videoOutput = DIGITAL_DISPLAY_PORT;
                        videoOutputInfo.hdcpVersion = HDCP_2_2;
                    } else if (strstr(dir->d_name, "DP")) {
                        videoOutputInfo.videoOutput = DIGITAL_DISPLAY_PORT;
                        videoOutputInfo.hdcpVersion = HDCP_2_2;
                    } else if (strstr(dir->d_name, "VGA")) {
                        videoOutputInfo.videoOutput = ANALOG_VGA;
                        videoOutputInfo.hdcpVersion = HDCP_NOT_APPLICABLE;
                    } else {
                        videoOutputInfo.videoOutput = DIGITAL_EXTERNAL_OTHER;
                        videoOutputInfo.hdcpVersion = HDCP_2_2;
                    }

                    videoOutputInfo.name = dir->d_name;
                    videoOutputs.push_back(videoOutputInfo);

                    if (!stat(statusPath.c_str(), &buf)) {
                        FILE * f = fopen(statusPath.c_str(), "rt");
                        std::vector<char> status(buf.st_size+1, 0);
                        size_t status_bytes_size = fread(&status[0], 1, status.size()-1, f);
                        NRDP_UNUSED(status_bytes_size);
                        fclose(f);

                        bool connected = strstr(&status[0], "connected") == &status[0];

                        if (connected) {
                            VideoOutputState videoOutputState;
                            videoOutputState.name = videoOutputInfo.name;
                            videoOutputState.videoOutput = videoOutputInfo.videoOutput;
                            videoOutputState.hdcpVersion = videoOutputInfo.hdcpVersion;

                            FILE * ef = fopen(edidPath.c_str(), "rb");
                            if (ef) {
                                videoOutputState.edid.resize(32*1024);
                                size_t edid_bytes_size = fread(&videoOutputState.edid[0], 1, videoOutputState.edid.size(), ef);
                                videoOutputState.edid.resize(edid_bytes_size);
                                fclose(ef);

                                // If this bit is set then the first DTD is native pixel format for EDID 1.3+
                                if (videoOutputState.edid[24] & (1 << 1)) {
                                    videoOutputState.displayWidthInPixels = videoOutputState.edid[54+2] |
                                                                            ((int)(videoOutputState.edid[54+4] & 0xF0) << 4);
                                    videoOutputState.displayHeightInPixels = videoOutputState.edid[54+5] |
                                                                            ((int)(videoOutputState.edid[54+7] & 0xF0) << 4);
                                }

                                // Ignore the fields if they are aspect ratio
                                if (videoOutputState.edid[21] && videoOutputState.edid[22]) {
                                    videoOutputState.displayWidthInCentimeters = videoOutputState.edid[21];
                                    videoOutputState.displayHeightInCentimeters = videoOutputState.edid[22];
                                }
                            }

                            Display* disp = XOpenDisplay(NULL);
                            Screen*  scrn = DefaultScreenOfDisplay(disp);
                            videoOutputState.outputWidthInPixels = scrn->width;
                            videoOutputState.outputHeightInPixels = scrn->height;
                            XCloseDisplay(disp);

                            activeVideoOutputs.push_back(videoOutputState);
                        }
                    }
                }
            }
        }
        closedir(d);
    }
#endif

    if (!videoOutputs.size() && !activeVideoOutputs.size()) {
        // supported video outputs
        VideoOutputInfo videoOutputInfo;
        videoOutputInfo.name = "TV Internal Display";
        videoOutputInfo.videoOutput = DIGITAL_INTERNAL;
        videoOutputInfo.hdcpVersion = HDCP_NOT_APPLICABLE;
        videoOutputs.push_back(videoOutputInfo);

        // currently active video outputs
        VideoOutputState videoOutputState;
        videoOutputState.name = "TV Internal Display";
        videoOutputState.videoOutput = DIGITAL_INTERNAL;
        videoOutputState.hdcpVersion = HDCP_NOT_APPLICABLE;
#ifdef __linux__
        Display* disp = XOpenDisplay(NULL);
        if (disp) {
            Screen* scrn = DefaultScreenOfDisplay(disp);
            if (scrn) {
                videoOutputState.outputWidthInPixels = scrn->width;
                videoOutputState.outputHeightInPixels = scrn->height;
                XCloseDisplay(disp);
            }
        }
#endif
#ifdef __APPLE__
    CGDirectDisplayID display = CGMainDisplayID();
    CGSize screensize = CGDisplayScreenSize(display);

    videoOutputState.displayWidthInCentimeters = screensize.width / 10;
    videoOutputState.displayHeightInCentimeters = screensize.height / 10;
    videoOutputState.outputWidthInPixels = CGDisplayPixelsWide(display);
    videoOutputState.outputHeightInPixels = CGDisplayPixelsHigh(display);
#endif
        activeVideoOutputs.push_back(videoOutputState);
    }
#endif
}
