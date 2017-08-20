/*
 * (c) 1997-2016 Netflix, Inc.  All content herein is protected by
 * U.S. copyright and other applicable intellectual property laws and
 * may not be copied without the express permission of Netflix, Inc.,
 * which reserves all rights.  Reuse of any of this content for any
 * purpose without the permission of Netflix, Inc. is strictly
 * prohibited.
 */

#include <cstring>
#include <string.h>
#include <unistd.h>

#include <nrd/AppLog.h>
#include <nrdbase/MutexRanks.h>
#include <nrdbase/ScopedMutex.h>
#include <nrd/IElementaryStreamPlayer.h>
#include <nrd/IDeviceError.h>
#include <nrdbase/Base64.h>

#include "ESPlayerConstants.h"
#include "PlayReadyDrmSystem.h"
#include "PlayReadyDrmSession.h"

//#define KIDFLIP_DEBUG

namespace netflix {
namespace device {

using namespace esplayer;

namespace { // anonymous

// The rights we want to request.
const DRM_WCHAR PLAY[] = { ONE_WCHAR('P', '\0'),
                           ONE_WCHAR('l', '\0'),
                           ONE_WCHAR('a', '\0'),
                           ONE_WCHAR('y', '\0'),
                           ONE_WCHAR('\0', '\0')
};
const DRM_CONST_STRING PLAY_RIGHT = CREATE_DRM_STRING(PLAY);
const DRM_CONST_STRING* RIGHTS[] = { &PLAY_RIGHT };

#ifdef KIDFLIP_DEBUG

// Converts a PR DRM_ID to a std::vector. Assumes everything is sized correctly.
std::vector<unsigned char> drmIdToVectorId(const DRM_ID& drmId)
{
    assert(sizeof(drmId.rgb) == DRM_ID_SIZE);
    return std::vector<unsigned char>(drmId.rgb, drmId.rgb + DRM_ID_SIZE);
}

std::string vectorToBase64String(const std::vector<uint8_t>& vec)
{
    const std::vector<unsigned char> vecB64(Base64::encode(vec));
    return std::string(vecB64.begin(), vecB64.end());
}

#endif // KIDFLIP_DEBUG

} // namespace anonymous

class LicenseResponse
{
public:
    LicenseResponse() : dlr(new DRM_LICENSE_RESPONSE) {}
    ~LicenseResponse() { delete dlr; }
    DRM_LICENSE_RESPONSE * get() { return dlr; }
    void clear() { ZEROMEM(dlr, sizeof(DRM_LICENSE_RESPONSE)); }
private:
    DRM_LICENSE_RESPONSE * const dlr;
};

/**
 * DRMPFNOUTPUTLEVELSCALLBACK for Drm_Reader_Bind. Sets the minimum
 * output protection levels.
 *
 * @param[in] outputLevels the output level data.
 * @param[in] callbackType the callback type.
 * @param[in] data the callback pass-thru data (DrmManager*).
 * @return DRM_SUCCESS if successful, DRM_FAILED if not.
 */
static DRM_RESULT outputLevelsCallback(const DRM_VOID *outputLevels,
                                       DRM_POLICY_CALLBACK_TYPE callbackType,
                                       const DRM_VOID *data)
{
    NRDP_UNUSED(data);
    NTRACE(TRACE_DRMSYSTEM,
           "outputLevelsCallback outputLevels=%p callbackType=%u data=%p",
           outputLevels, static_cast<uint32_t>(callbackType), data);

    // We only care about the play callback.
    if (callbackType != DRM_PLAY_OPL_CALLBACK)
        return DRM_SUCCESS;

    // Pull out the protection levels.
    PlayReadyDrmSession::PlayLevels* levels = static_cast<PlayReadyDrmSession::PlayLevels*>(const_cast<DRM_VOID*>(data));
    const DRM_PLAY_OPL_EX* playLevels = static_cast<const DRM_PLAY_OPL_EX*>(outputLevels);
    levels->compressedDigitalVideoLevel_ = playLevels->minOPL.wCompressedDigitalVideo;
    levels->uncompressedDigitalVideoLevel_ = playLevels->minOPL.wUncompressedDigitalVideo;
    levels->analogVideoLevel_ = playLevels->minOPL.wAnalogVideo;
    levels->compressedDigitalAudioLevel_ = playLevels->minOPL.wCompressedDigitalAudio;
    levels->uncompressedDigitalAudioLevel_ = playLevels->minOPL.wUncompressedDigitalAudio;

    // At actual device, enable/disable device output protection will be needed
    // upon receiving this protection information.
    NTRACE(TRACE_DRMSYSTEM, "Output Protection Level : compressed digital %d, uncompressed digital %d, analog video %d",
           levels->compressedDigitalVideoLevel_,
           levels->uncompressedDigitalVideoLevel_,
           levels->analogVideoLevel_);

    // All done.
    return DRM_SUCCESS;
}

PlayReadyDrmSession::PlayReadyDrmSession(uint32_t sessionId,
                                         std::shared_ptr<PlayReadyDrmSystem> drmSystem)
  :mSessionState(InvalidState)
  ,mSessionId(sessionId)
  ,mDrmSystem(drmSystem)
  ,mDrmDecryptContextMutex(DRMCONTEXT_MUTEX, "PlayreadyDrmSession::mDrmDecryptContextMutex")
  ,mLicenseResponse(new LicenseResponse())
{
    NTRACE(TRACE_DRMSYSTEM, "sessionId %d", sessionId);
    
    // Assign app context(DRM_APP_CONTEXT) to mAppContext
    mAppContext = mDrmSystem->getDrmAppContext();
    mSecureStopId.clear();
}

PlayReadyDrmSession::PlayReadyDrmSession(uint32_t sessionId,
                                         std::string contentId,
                                         enum DrmLicenseType licenseType,
                                         const std::vector<uint8_t>& drmHeader,
                                         std::shared_ptr<PlayReadyDrmSystem> drmSystem)
  :mSessionState(InvalidState)
  ,mSessionId(sessionId)
  ,mContentId(contentId)
  ,mLicenseType(licenseType)
  ,mDrmSystem(drmSystem)
  ,mDrmDecryptContextMutex(DRMCONTEXT_MUTEX, "PlayreadyDrmSession::mDrmDecryptContextMutex")
  ,mLicenseResponse(new LicenseResponse())
{
    // copy drm header as session information
    mDrmHeader = drmHeader;
    NTRACE(TRACE_DRMSYSTEM, "sessionId %d, licenseType %s, contentId %s, drmHeader.size() %zu",
           sessionId, drmLicenseTypeToString(licenseType).c_str(), Base64::encode(contentId).c_str(), drmHeader.size());
#ifdef KIDFLIP_DEBUG
    Log::warn(TRACE_DRMSYSTEM, "==== PlayReadyDrmSystem %3d ctor: KID %s", sessionId, Base64::encode(contentId).c_str());
#endif
    // Assign app context(DRM_APP_CONTEXT) to mAppContext
    mAppContext = mDrmSystem->getDrmAppContext();
    mSecureStopId.clear();
}

PlayReadyDrmSession::~PlayReadyDrmSession()
{
    NTRACE(TRACE_DRMSYSTEM, "%s sessionId %d", __func__, mSessionId);
    finalizeDrmSession();
    mDrmHeader.clear();
    mSecureStopId.clear();
}

void PlayReadyDrmSession::finalizeDrmSession()
{
    ScopedMutex decryptContextLock(mDrmDecryptContextMutex);
    {
        ScopedMutex systemLock(mDrmSystem->getDrmAppContextMutex());

        NTRACE(TRACE_DRMSYSTEM, "%s sessionId:%d, licenseType:%s", __func__, mSessionId, drmLicenseTypeToString(mLicenseType).c_str());

        if( getSessionState() == IDrmSession::LicenseAcquisitionState){
            // cancel license challenge
            cancelChallenge();
        } else if (getSessionState() != IDrmSession::InvalidState ){
            // cleanup any context created
            cleanupDecryptContext();
        }
        setSessionState(IDrmSession::InvalidState);
    }
}

std::string PlayReadyDrmSession::getDrmType()
{
    return "PlayReady";
}

uint32_t PlayReadyDrmSession::getSessionId()
{
    return mSessionId;
}

std::string PlayReadyDrmSession::getContentId()
{
    return mContentId;
}

enum DrmLicenseType PlayReadyDrmSession::getLicenseType()
{
    return mLicenseType;
}

void PlayReadyDrmSession::setSessionState(enum IDrmSession::SessionState sessionState)
{
    NTRACE(TRACE_DRMSYSTEM, "%s from %d to %d for sessionId:%d, licenseType:%s",
           __func__, mSessionState, sessionState, mSessionId, drmLicenseTypeToString(mLicenseType).c_str());
    mSessionState = sessionState;
}

enum IDrmSession::SessionState PlayReadyDrmSession::getSessionState()
{
    return mSessionState;
}


/**
 * Generates challenge data for a particular protection system and system
 * specific data. Indicates whether a license has already been stored for
 * this data or whether additional challenge stages are required.
 *
 * The challenge data is allocated and stored in the challengeData argument
 * and will be deallocated by the caller.
 *
 *
 * @param[in] protectionSystemID: the UUID identifying the protection system
 * @param[in] systemSpecificData: data needed to generate the challenge.
 * @param[in] systemSpecificDataLen:
 * @param[out] challengedata
 *
 * @return OK if challenge data is successfully generated.
 *         LICENSE_ALREADY_STORED if no additional license acquistion is needed.
 *         ERROR if challenge generation has failed.
 */
NFErr PlayReadyDrmSession::getChallengeData(std::vector<uint8_t>& challenge,
                                            const std::string& contentId,
                                            enum DrmLicenseType licenseType,
                                            const std::vector<uint8_t>& drmHeader)
{
    // copy drm header as session information
    mDrmHeader = drmHeader;
    mContentId = contentId;
    mLicenseType = licenseType;
    return getChallengeData(challenge, (licenseType == LIMITED_DURATION_LICENSE)? true: false);
}

NFErr PlayReadyDrmSession::getChallengeData(std::vector<uint8_t>& challenge, bool isLDL)
{
    DRM_RESULT err;

    NTRACE(TRACE_DRMSYSTEM, "PlayReadyDrmSession::generateChallenge challenge=%p sessionId %d"
           , &challenge, mSessionId);

    // open scope for DRM_APP_CONTEXT mutex
    ScopedMutex systemLock(mDrmSystem->getDrmAppContextMutex());

    // sanity check for drm header
    if (mDrmHeader.size() == 0){
        return NFErr_NotAllowed;
    }

    // Application wide context check. PlayReadyDrmSystem should have initialized
    // DRM__APP_CONTEXT for application level. If this DRM_APP_CONTEXT does not
    // exist, each session should recover it by initializing DRM_APP_CONTEXT.
    if( mAppContext.get() == NULL) {
        NTRACE(TRACE_DRMSYSTEM, "PlayReadyDrmSession::%s: recovering application wide context", __func__);

        NTRACE(TRACE_DRMSYSTEM, "cleanup Key/Drm stores before Drm_Initialize");
        // Cleanup drm store and key store
        if (mDrmSystem->deleteDrmStore() != NFErr_OK ){
            NTRACE(TRACE_DRMSYSTEM, "deleteDrmStore failed before Drm_Initialize");
        }
        if (mDrmSystem->deleteKeyStore() != NFErr_OK){
            NTRACE(TRACE_DRMSYSTEM, "deleteKeyStore failed before Drm_Initialize");
        }

        NFErr error = mDrmSystem->initialize();
        if (error != NFErr_OK){
            return error;
        }
        // Assign app context(DRM_APP_CONTEXT) to mAppContext
        mAppContext = mDrmSystem->getDrmAppContext();
    } else {
        // reinitialze DRM_APP_CONTEXT - this is limitation of PlayReady 2.x
        err = Drm_Reinitialize(mAppContext.get());
        if(DRM_FAILED(err))
        {
            Log::error(TRACE_DRMSYSTEM, "PlayReadyDrmSession::initDrmContext: Error in Drm_Reinitialize:"
                       " 0x%08lx", static_cast<unsigned long>(err));
            std::ostringstream ss;
            ss << "PlayReadyDrmSession::initDrmContext: Error in Drm_Reinitialize:" << std::showbase << std::hex << err;
            Variant errInfo;
            errInfo["errorDescription"] = ss.str();
            return NFErr(new IDeviceError(DRMSYSTEM_INITIALIZATION_ERROR, err, errInfo));
        }
    }

    /*
     * Set the drm context's drm header property to the systemSpecificData.
     */
    NTRACE(TRACE_DRMSYSTEM, "Drm_Content_SetProperty : generateChallenge setting header");
    err = Drm_Content_SetProperty(mAppContext.get(),
                                  DRM_CSP_AUTODETECT_HEADER,
                                  &mDrmHeader[0],
                                  mDrmHeader.size());
    if (DRM_FAILED(err))
    {
        Log::error(TRACE_DRMSYSTEM, "PlayReadyDrmSession::generateChallenge: Error in Drm_Content_SetProperty:"
                   " 0x%08lx", static_cast<unsigned long>(err));
        std::ostringstream ss;
        ss << "PlayReadyDrmSession::generateChallenge: Error in Drm_Content_SetProperty:"
           << std::showbase << std::hex << err;
    Variant errInfo;
        errInfo["errorDescription"] = ss.str();
        return NFErr(new IDeviceError(DRMSYSTEM_CHALLENGE_ERROR, err, errInfo));
    }

    // Figure out the buffer allocation sizes for the challenge.
    NTRACE(TRACE_DRMSYSTEM, "generateChallenge querying challenge size");
    DRM_DWORD challengeSize = 0;
    mNounce.resize(TEE_SESSION_ID_LEN);
    err = Drm_LicenseAcq_GenerateChallenge_Netflix(mAppContext.get(),
                                                   RIGHTS,
                                                   sizeof(RIGHTS) / sizeof(DRM_CONST_STRING*),
                                                   NULL,
                                                   NULL, 0,
                                                   NULL, NULL,
                                                   NULL, NULL,
                                                   NULL, &challengeSize,
                                                   &mNounce[0], isLDL);
    if (err != DRM_E_BUFFERTOOSMALL)
    {
        Log::error(TRACE_DRMSYSTEM, "PlayReadyDrmSession::generateChallenge: Error in "
                   " Drm_LicenseAcq_GenerateChallenge: 0x%08lx",
                   static_cast<unsigned long>(err));
        std::ostringstream ss;
        ss << "PlayReadyDrmSession::generateChallenge: Error in Drm_LicenseAcq_GenerateChallenge:"
           << std::showbase << std::hex << err;
        Variant errInfo;
        errInfo["errorDescription"] = ss.str();
        return NFErr(new IDeviceError(DRMSYSTEM_CHALLENGE_ERROR, err, errInfo));
    }

    // Now get the challenge.
    NTRACE(TRACE_DRMSYSTEM, "PlayReadyDrmSession::generateChallenge: generating challenge size=%u",
           static_cast<uint32_t>(challengeSize));
    challenge.resize(challengeSize);

    err = Drm_LicenseAcq_GenerateChallenge_Netflix(mAppContext.get(),
                                                   RIGHTS,
                                                   sizeof(RIGHTS) / sizeof(DRM_CONST_STRING*),
                                                   NULL,
                                                   NULL, 0,
                                                   NULL, NULL,
                                                   NULL, NULL,
                                                   (DRM_BYTE*)&challenge[0],
                                                   &challengeSize,
                                                   &mNounce[0],
                                                   isLDL);
    if (DRM_FAILED(err))
    {
        Log::error(TRACE_DRMSYSTEM, "generateChallenge: Error in Drm_LicenseAcq_GenerateChallenge: 0x%08lx", static_cast<unsigned long>(err));
        std::ostringstream ss;
        ss << "PlayReadyDrmSession::generateChallenge: Error in Drm_LicenseAcq_GenerateChallenge:"
           << std::showbase << std::hex << err;
        Variant errInfo;
        errInfo["errorDescription"] = ss.str();
        return NFErr(new IDeviceError(DRMSYSTEM_CHALLENGE_ERROR, err, errInfo));
    }

    // All done.
    challenge.resize(challengeSize);

    return NFErr_OK;
}

/**
 * Stores license and returns a session ID.  The session ID will be used to
 * identify secure stops.
 *
 * @param[in] licenseData: the license data
 * @param[in] licenseDataSize: license data size.
 * @param[out] sessionID: specifies the established DRM session.
 *
 * @return OK on success, FURTHER_STAGES_REQUIRED if this is a
 *         multi-stage challenge and additional stages are needed, or
 *         ERROR.
 */
NFErr PlayReadyDrmSession::storeLicenseData(const std::vector<uint8_t>& licenseData,
                                            std::vector<unsigned char> &secureStopId)
{
    NTRACE(TRACE_DRMSYSTEM, "PlayReadyDrmSession::storeLicense data=%p sessionId %d, licenseType %s"
           , &licenseData, mSessionId, drmLicenseTypeToString(mLicenseType).c_str());

    // open scope for DRM_APP_CONTEXT mutex
    ScopedMutex systemLock(mDrmSystem->getDrmAppContextMutex());

    unsigned char uuid[TEE_SESSION_ID_LEN];
    // Even if this is output buffer, we zero it out for safety since we do not
    // know how PlayReady handles the output buffer.
    ZEROMEM(uuid, TEE_SESSION_ID_LEN);

    std::vector<uint8_t> localLicenseData = licenseData;
    
    DRM_RESULT err;

    // reinitialze DRM_APP_CONTEXT and set DRM header for current session
    NFErr nferr = reinitializeForCurrentSession();
    if(!nferr.ok()){
        return nferr;
    }
    
    /*
     * Store the license
     */
    // playready2.5-ss-tee which has LDL support - new function added
    NTRACE(TRACE_DRMSYSTEM, "PlayReadyDrmSession::storeLicense mAppContext.get():%p, licenseData:%p, licenseSize:%d, uuid:%p, resp:%p",
           mAppContext.get(),
           const_cast<uint8_t*>(&licenseData[0]),
           (DRM_DWORD)licenseData.size(),
           uuid,
           mLicenseResponse->get());
    
    mLicenseResponse->clear();
    err = Drm_LicenseAcq_ProcessResponse_Netflix(mAppContext.get(),
                                                 DRM_PROCESS_LIC_RESPONSE_NO_FLAGS,
                                                 NULL, NULL,
                                                 &localLicenseData[0],
                                                 (DRM_DWORD)localLicenseData.size(),
                                                 uuid,
                                                 mLicenseResponse->get());
    if (DRM_FAILED(err)) {
        Log::error(TRACE_DRMSYSTEM, "PlayReadyDrmSession::storeLicense: Error in "
                   "Drm_LicenseAcq_ProcessResponse: 0x%08lx",
                   static_cast<unsigned long>(err));
        std::ostringstream ss;
        ss << "PlayReadyDrmSession::storeLicense: Error in Drm_LicenseAcq_ProcessResponse:"
           << std::showbase << std::hex << err;
        Variant errInfo;
        errInfo["errorDescription"] = ss.str();
        return NFErr(new IDeviceError(DRMSYSTEM_LICENSE_ERROR, err, errInfo));
    }

#ifdef KIDFLIP_DEBUG
    const DRM_LICENSE_RESPONSE * const licresp = mLicenseResponse->get();
    const DRM_LICENSE_ACK licAck = licresp->m_rgoAcks[0];
    const DRM_ID drmId = licAck.m_oKID;
    const std::vector<unsigned char> kidVec = drmIdToVectorId(drmId);
    Log::warn(TRACE_DRMSYSTEM, "==== PlayReadyDrmSystem %3d STOR: KID %s", mSessionId, vectorToBase64String(kidVec).c_str());
#endif

    // convert uuid to vector
    secureStopId.clear();
    secureStopId.resize(TEE_SESSION_ID_LEN);
    secureStopId.assign(uuid, uuid + TEE_SESSION_ID_LEN);
    mSecureStopId = secureStopId;

    // All done.
    return NFErr_OK;
}

/**
 * Clear a previously stored DRM license.
 *
 * @return OK if succeeded, otherwise ERROR.
 */
NFErr PlayReadyDrmSession::clearLicense()
{
    NTRACE(TRACE_DRMSYSTEM, "PlayReadyDrmSession::clearLicense");

    // open scope for DRM_APP_CONTEXT mutex
    ScopedMutex systemLock(mDrmSystem->getDrmAppContextMutex());

    DRM_RESULT result;
    DRM_DWORD numDeleted;

    NFErr nferr = reinitializeForCurrentSession();
    if(!nferr.ok()){
        return nferr;
    }

    /*
     * Delete license for this KID (from drm header)
     */
    result = Drm_StoreMgmt_DeleteLicenses(mAppContext.get(), NULL, &numDeleted);
    if (DRM_FAILED(result))
    {
        Log::error(TRACE_DRMSYSTEM, "PlayReadyDrmSession::clearLicense: Error in "
                   "Drm_StoreMgmt_DeleteLicenses: 0x%08lx",
                   static_cast<unsigned long>(result));
        std::ostringstream ss;
        ss << "PlayReadyDrmSession::clearLicense: Error in Drm_StoreMgmt_DeleteLicenses:"
           << std::showbase << std::hex << result;
        Variant errInfo;
        errInfo["errorDescription"] = ss.str();
        return NFErr(new IDeviceError(DRMSYSTEM_LICENSE_ERROR, result, errInfo));
    }

    return NFErr_OK;
}


NFErr PlayReadyDrmSession::initDecryptContext()
{
    NTRACE(TRACE_DRMSYSTEM,
           "PlayReadyDrmSession::initDecryptContext sessionId %d, licenseType %s",
           mSessionId, drmLicenseTypeToString(mLicenseType).c_str());

    // open scope for DRM_APP_CONTEXT mutex
    ScopedMutex systemLock(mDrmSystem->getDrmAppContextMutex());

    DRM_RESULT err;

    /*
     * reinitialze DRM_APP_CONTEXT and set DRM header for current session for
     * simulataneous decryption support
     */
    NFErr nferr = reinitializeForCurrentSession();
    if(!nferr.ok()){
        return nferr;
    }

    /*
     * Create a decrypt context and bind it with the drm context.
     */
    if (decryptContext_.get()){
        // we already have initialized decrypt context.
        NTRACE(TRACE_DRMSYSTEM, "PlayReadyDrmSession::initDecryptContext: decrypContext already initialized");
        return NFErr_OK;
    }
    decryptContext_.reset(new DRM_DECRYPT_CONTEXT);
    memset(decryptContext_.get(), 0, sizeof(*decryptContext_));

    if(mSecureStopId.size() == TEE_SESSION_ID_LEN ){
        err = Drm_Reader_Bind_Netflix(mAppContext.get(),
                                      RIGHTS,
                                      sizeof(RIGHTS) / sizeof(DRM_CONST_STRING*),
                                      &outputLevelsCallback, &levels_,
                                      &mSecureStopId[0],
                                      decryptContext_.get());
    } else {
        Log::error(TRACE_DRMSYSTEM, "PlayReadyDrmSession::initDecryptContext: Error securestopId not valid");
        decryptContext_.reset();
        std::ostringstream ss;
        ss << "PlayReadyDrmSession::initDecryptContext: Error securestopId not valid";
        Variant errInfo;
        errInfo["errorDescription"] = ss.str();
        return NFErr(new IDeviceError(DECRYPTION_ERROR, AgrippinaAppDeviceSpecific_InvalidDrmSecureStopId, errInfo));
    }

    if (DRM_FAILED(err))
    {
        Log::error(TRACE_DRMSYSTEM, "PlayReadyDrmSession::initDecryptContext: Error in Drm_Reader_Bind"
                   " 0x%08lx", static_cast<unsigned long>(err));
        decryptContext_.reset();
        std::ostringstream ss;
        ss << "PlayReadyDrmSession::initDecryptContext:Error in Drm_Reader_Bind "
           << std::showbase << std::hex << err;
        Variant errInfo;
        errInfo["errorDescription"] = ss.str();
        return NFErr(new IDeviceError(DECRYPTION_ERROR, err, errInfo));
    }

    err = Drm_Reader_Commit(mAppContext.get(), &outputLevelsCallback, &levels_);
    if (DRM_FAILED(err))
    {
        Log::error(TRACE_DRMSYSTEM, "PlayReadyDrmSession::initDecryptContext: Error inDrm_Reader_Commit 0x%08lx", static_cast<unsigned long>(err));
        decryptContext_.reset();
        std::ostringstream ss;
        ss << "PlayReadyDrmSession::initDecryptContext:Error inDrm_Reader_Commit"
           << std::showbase << std::hex << err;
        Variant errInfo;
        errInfo["errorDescription"] = ss.str();
        return NFErr(new IDeviceError(DECRYPTION_ERROR, err, errInfo));
    }
    return NFErr_OK;
}

NFErr PlayReadyDrmSession::decrypt(unsigned char* IVData,
                                   uint32_t IVDataSize,
                                   ullong byteOffset,
                                   DataSegment& dataSegment)
{
    // decrypt does not use DRM_APP_CONTEXT, so, it should be safe to use
    // different mutex between what protect DRM_APP_CONTEXT and
    // DRM_DECRYPT_CONTEXT. However, underlying secure stop related code may be
    // vulnarable to sharing those, so here, let's protect decrypt context also
    // with mutext that protect DRM_APP_CONTEXT

    // open scope for DRM_APP_CONTEXT mutex
    ScopedMutex systemLock(mDrmSystem->getDrmAppContextMutex());

    NRDP_UNUSED(IVDataSize);
    assert(IVDataSize > 0);
    unsigned char* data = dataSegment.data;
    uint32_t size = dataSegment.numBytes;
    if(size == 0){
        return NFErr_OK;
    }

    //NTRACE(TRACE_DRMSYSTEM, "PlayReadyDrmSession::decrypt data=%p length=%d", data, size);

    // If we don't yet have a decryption context, get one from the session.
    if (!decryptContext_.get()) {
        std::ostringstream ss;
        ss << "PlayReadyDrmSession::decrypt: we don't yet have a decryption context:";
        Variant errInfo;
        errInfo["errorDescription"] = ss.str();
        return NFErr(new IDeviceError(DECRYPTION_ERROR,
                                      AgrippinaAppDeviceSpecific_NoDecryptionContext,
                                      errInfo));
    }

    // Initialize the decryption context for Cocktail packaged
    // content. This is a no-op for AES packaged content.
    DRM_RESULT err = DRM_SUCCESS;
    if (size <= 15)
    {
        err = Drm_Reader_InitDecrypt(decryptContext_.get(),
                                     (DRM_BYTE*)data, size);
    }
    else
    {
        err = Drm_Reader_InitDecrypt(decryptContext_.get(),
                                     (DRM_BYTE*)(data + size - 15), size);
    }
    if (DRM_FAILED(err))
    {
        Log::error(TRACE_DRMSYSTEM, "PlayReadyDrmSession::decrypt: error in Drm_Reader_InitDecrypt:"
                   " 0x%08lx",static_cast<unsigned long>(err));
        std::ostringstream ss;
        ss << "PlayReadyDrmSession::decrypt: Error in Drm_Reader_InitDecrypt:"
           << std::showbase << std::hex << err;
        Variant errInfo;
        errInfo["errorDescription"] = ss.str();
        return NFErr(new IDeviceError(DECRYPTION_ERROR, err, errInfo));
    }

    // Decrypt.

    // The documentation indicates the AES Counter-mode context
    // pointer should be NULL for Cocktail packaged content, but
    // it is ignored so we can pass it in anyway.

    //printf("IVSize %d,\t byteOffset %lld\n", IVDataSize, byteOffset);

    DRM_AES_COUNTER_MODE_CONTEXT ctrContext;
    if (IVData && IVDataSize == 8) {
        NETWORKBYTES_TO_QWORD(ctrContext.qwInitializationVector, IVData, 0);
        ctrContext.qwBlockOffset = byteOffset >> 4;
        ctrContext.bByteOffset = (DRM_BYTE)(byteOffset & 0xf);
    } else if (IVData && IVDataSize == 16) {
        NETWORKBYTES_TO_QWORD(ctrContext.qwInitializationVector, IVData, 0);
        NETWORKBYTES_TO_QWORD(byteOffset, IVData, 8);
        ctrContext.qwBlockOffset = byteOffset;
        ctrContext.bByteOffset = 0;
    } else  {
        ctrContext.qwInitializationVector = 0;
        ctrContext.qwBlockOffset = byteOffset >> 4;
        ctrContext.bByteOffset = (DRM_BYTE)(byteOffset & 0xf);
    }

    //printf("IVSize %d,\t byteOffset %lld,\t qwBlockOffset %lld,\t bByteOffset %d, numBytes %d\n",
    //       IVDataSize, byteOffset, ctrContext.qwBlockOffset, ctrContext.bByteOffset, dataSegment.numBytes);

    err = Drm_Reader_Decrypt(decryptContext_.get(), &ctrContext,
                             (DRM_BYTE*)dataSegment.data,
                             dataSegment.numBytes);
    if (DRM_FAILED(err))
    {
        Log::error(TRACE_DRMSYSTEM, "PlayReadyDrmSession::decrypt: error in Drm_Reader_Decrypt:"
                   " 0x%08lx", static_cast<unsigned long>(err));
        std::ostringstream ss;
        ss << "PlayReadyDrmSession::decrypt: Error in Drm_Reader_Decrypt:" << std::showbase << std::hex << err;
        Variant errInfo;
        errInfo["errorDescription"] = ss.str();
        return NFErr(new IDeviceError(DECRYPTION_ERROR, err, errInfo));
    }
    // All good.
    return NFErr_OK;
}

void PlayReadyDrmSession::cleanupDecryptContext()
{
    NTRACE(TRACE_DRMSYSTEM,
           "PlayReadyDrmSession::cleanupDecryptContext sessionId %d, licenseType %s",
           mSessionId, drmLicenseTypeToString(mLicenseType).c_str());

    // mutex is set at finalizeDrmSession

    DRM_RESULT err;
    if(decryptContext_.get()){
        /*
         * unbind decrypt context from drm context
         */
        err = Drm_Reader_Unbind(mAppContext.get(), decryptContext_.get());
        if (DRM_FAILED(err)) {
            NTRACE(TRACE_DRMSYSTEM, "PlayReadyDrmSession::cleanupDecryptContext: Error in "
                   "Drm_Reader_Unbind: 0x%08lx",
                   static_cast<unsigned long>(err));
        }
    } else {
        // This means that we created DrmSession, but never bound it to
        // DRM_DECRYPT_CONTEXT. In this case, if we have a license to bind, we
        // need to bind and unbind to create a secure stop.  This is requirement
        // from security team.
        NTRACE(TRACE_DRMSYSTEM, "DecryptContext not created: temporarily bind and unbind");

        reinitializeForCurrentSession();

        // now bind and unbind it
        //const DRM_CONST_STRING* RIGHTS[] = { &PLAY_RIGHT };
        decryptContext_.reset(new DRM_DECRYPT_CONTEXT);
        memset(decryptContext_.get(), 0, sizeof(*decryptContext_));

        if(mSecureStopId.size() == TEE_SESSION_ID_LEN ){
            err = Drm_Reader_Bind_Netflix(mAppContext.get(),
                                          RIGHTS,
                                          sizeof(RIGHTS) / sizeof(DRM_CONST_STRING*),
                                          &outputLevelsCallback, &levels_,
                                          &mSecureStopId[0],
                                          decryptContext_.get());
        } else {
            Log::error(TRACE_DRMSYSTEM, "PlayReadyDrmSession::cleanupDecryptContext:Error securestopId not valid");
            decryptContext_.reset();
            return;
        }

        if (DRM_FAILED(err)) {
            // this is not happening during playback. No need to logblob
            Log::error(TRACE_DRMSYSTEM, "Error Drm_Reader_Bind: 0x%08lx while creating temporary DRM_DECRYPT_CONTEXT",
                       static_cast<unsigned long>(err));
            decryptContext_.reset();
            return ;
        }

        err = Drm_Reader_Unbind(mAppContext.get(), decryptContext_.get());
        if (DRM_FAILED(err))
        {
            Log::error(TRACE_DRMSYSTEM, "Error Drm_Reader_Unbind: 0x%08lx when creating temporary DRM_DECRYPT_CONTEXT",
                       static_cast<unsigned long>(err));
        }
    }

    if(mAppContext.get()){
        err = Drm_Reader_Commit(mAppContext.get(), NULL, NULL);
        if(DRM_FAILED(err)){
            // nothing that we can do about. Just log
            Log::warn(TRACE_DRMSYSTEM, "PlayReadyDrmSystem::%s Drm_Reader_Commit 0x%08lx", __func__, static_cast<unsigned long>(err));
        }
    }

    decryptContext_.reset();
}

void PlayReadyDrmSession::cancelChallenge()
{
    // scoped mutex is set at finalizeDrmSession
    NTRACE(TRACE_DRMSYSTEM, "%s sessionId %d, licenseType %s", __func__, mSessionId, drmLicenseTypeToString(mLicenseType).c_str());

    DRM_RESULT err;
    err = Drm_LicenseAcq_CancelChallenge_Netflix(mAppContext.get(), &mNounce[0]);
    if (DRM_FAILED(err)) {
        Log::error(TRACE_DRMSYSTEM, "Error Drm_LicenseAcq_CancelChallenge_Netflix: 0x%08lx",
                   static_cast<unsigned long>(err));
    }
}

NFErr PlayReadyDrmSession::reinitializeForCurrentSession()
{
    /*
     * Simultaneous decryption support - when multiple license requests were in
     * flight, and license were retrived and ready to be stored, we should
     * reinitialize app context for current license to be stored
     */

    NFErr nferr = NFErr_OK;
    DRM_RESULT err;
    err = Drm_Reinitialize(mAppContext.get());
    if (DRM_FAILED(err)) {
        NTRACE(TRACE_DRMSYSTEM, "PlayReadyDrmSession::storeLicense: Error in "
               "Drm_Reinitialize: 0x%08lx",
               static_cast<unsigned long>(err));
        std::ostringstream ss;
        ss << "PlayReadyDrmSession::" << __func__
           << " Error in Drm_Reinitialize:"
           << std::showbase << std::hex << err;
        Variant errInfo;
        errInfo["errorDescription"] = ss.str();
        return NFErr(new IDeviceError(DRMSYSTEM_INITIALIZATION_ERROR, err, errInfo));
    }

    err = Drm_Content_SetProperty(mAppContext.get(),
                                  DRM_CSP_AUTODETECT_HEADER,
                                  &mDrmHeader[0],
                                  mDrmHeader.size());
    if (DRM_FAILED(err)) {
        Log::error(TRACE_DRMSYSTEM, "PlayReadyDrmSession::storeLicense: Error in "
                   "Drm_Content_SetProperty: 0x%08lx",
                   static_cast<unsigned long>(err));
        std::ostringstream ss;
        ss << "PlayReadyDrmSession::" << __func__
           << " Error in Drm_Content_SetProperty:"
           << std::showbase << std::hex << err;
        Variant errInfo;
        errInfo["errorDescription"] = ss.str();
        return NFErr(new IDeviceError(DRMSYSTEM_INITIALIZATION_ERROR, err, errInfo));
    }
    return nferr;
}

Mutex& PlayReadyDrmSession::getDecryptContextMutex()
{
    return mDrmDecryptContextMutex;
}

}} // namespace netflix::device
