/*
 * (c) 1997-2016 Netflix, Inc.  All content herein is protected by
 * U.S. copyright and other applicable intellectual property laws and
 * may not be copied without the express permission of Netflix, Inc.,
 * which reserves all rights.  Reuse of any of this content for any
 * purpose without the permission of Netflix, Inc. is strictly
 * prohibited.
 */

#ifndef PLAYREADY3
#error "You must define PLAYREADY3 to use this file"
#endif

#include "PlayReady3DrmSession.h"

#include <nrd/AppLog.h>
#include <nrdbase/ScopedMutex.h>
#include <nrdbase/Base64.h>

#include <drmconstants.h>
#include <drmbytemanip.h>
#include <drmsecurestoptypes.h>

#include "ESPlayerConstants.h"
#include "PlayReady3DrmSystem.h"
#include "PlayReady3MeteringCert.h"

#include <algorithm>
#include <byteswap.h>

//#define KIDFLIP_DEBUG

// TODO: Enter a JIRA to remove DONT_ACCEPT_BOTH_KID_FORMATS conditional code,
// keeping the positive case (when DONT_ACCEPT_BOTH_KID_FORMATS is defined).

namespace netflix {
namespace device {

using namespace esplayer;

namespace {

// The rights we want to request.
const DRM_WCHAR PLAY[] = { DRM_ONE_WCHAR('P', '\0'),
                           DRM_ONE_WCHAR('l', '\0'),
                           DRM_ONE_WCHAR('a', '\0'),
                           DRM_ONE_WCHAR('y', '\0'),
                           DRM_ONE_WCHAR('\0', '\0')
};
const DRM_CONST_STRING PLAY_RIGHT = DRM_CREATE_DRM_STRING(PLAY);
const DRM_CONST_STRING* RIGHTS[] = { &PLAY_RIGHT };

#if defined(DUMPING_BIND_CALLBACK)

    void dumpBindCallback(DRM_VOID const * cb, DRM_POLICY_CALLBACK_TYPE type);
    std::string toJson(DRM_DWORD x, size_t width = 0);
    std::string toJson(DRM_VOID * x);
    std::string toJson(DRM_GUID const & x);
    std::string toJson(DRM_OUTPUT_PROTECTION_EX const & x);
    std::string toJson(DRM_XB_UNKNOWN_OBJECT const & x);
    std::string toJson(DRM_XMRFORMAT const & x);
    std::string toJson(DRM_MINIMUM_OUTPUT_PROTECTION_LEVELS const & x);
    std::string toJson(DRM_OPL_OUTPUT_IDS const & x);
    std::string toJson(DRM_VIDEO_OUTPUT_PROTECTION_IDS_EX const & x);
    std::string toJson(DRM_AUDIO_OUTPUT_PROTECTION_IDS_EX const & x);
    std::string toJson(DRM_PLAY_OPL_EX2 const & opl);
    std::string toJson(DRM_EXTENDED_RESTRICTION_CALLBACK_STRUCT const & r);

    template <typename T>
    std::string toJson(size_t n, T const * a)
{
        std::ostringstream ss;
        ss << '[';
        if ((n > 0) && (a != NULL)) {
            ss << toJson(a[0]);
            for (size_t i = 1; i < n; ++i) {
                ss << ',' << toJson(a[i]);
            }
        }
        ss << ']';
        return ss.str();
    }

#endif // defined(DUMPING_BIND_CALLBACK)

DRM_RESULT bindCallback(const DRM_VOID           *f_pvCallbackData,
                        DRM_POLICY_CALLBACK_TYPE  f_dwCallbackType,
                        const DRM_KID            *f_pKID,
                        const DRM_LID            *f_pLID,
                        const DRM_VOID           *f_pv)
{
    // FIXME: Also handle HDCP Type Restriction, DigitalToken, or others?

    NRDP_UNUSED(f_pKID);
    NRDP_UNUSED(f_pLID);

    assert(f_pv);
    assert(f_pvCallbackData);

#if defined(DUMPING_BIND_CALLBACK)
    dumpBindCallback(f_pvCallbackData, f_dwCallbackType);
#endif

    DRM_RESULT dr = DRM_SUCCESS;
    switch (f_dwCallbackType)
    {
        case DRM_PLAY_OPL_CALLBACK:
        {
            NTRACE(TRACE_DRMSYSTEM, "DRM_PLAY_OPL_CALLBACK");
            OutputProtection * const op = const_cast<OutputProtection *>(static_cast<const OutputProtection*>(f_pv));
            const DRM_PLAY_OPL_EX2 * const opl = static_cast<const DRM_PLAY_OPL_EX2 *>(f_pvCallbackData);
            assert(opl->dwVersion == 0);

            // Output protection levels
            op->setOutputLevels(opl->minOPL);
            NTRACE(TRACE_DRMSYSTEM, "Output Protection Levels:\n\t"
                    "comp dig video  : %d\n\t"
                    "uncomp dig video: %d\n\t"
                    "analog video    : %d\n\t"
                    "comp dig audio  : %d\n\t"
                    "uncomp dig audio: %d",
                op->compressedDigitalVideoLevel,
                op->uncompressedDigitalVideoLevel,
                op->analogVideoLevel,
                op->compressedDigitalAudioLevel,
                op->uncompressedDigitalAudioLevel);

            // MaxRes Decode
            const DRM_VIDEO_OUTPUT_PROTECTION_IDS_EX &dvopi = opl->dvopi;
            for (size_t i = 0; i < dvopi.cEntries; ++i)
            {
                const DRM_OUTPUT_PROTECTION_EX &dope = dvopi.rgVop[i];
                if (DRM_IDENTICAL_GUIDS(&dope.guidId, &g_guidMaxResDecode))
                {
                    assert(dope.dwVersion == 3);
                    uint32_t mrdWidth, mrdHeight;
                    const int inc = sizeof(uint32_t);
                    assert(dope.cbConfigData >= 2*inc);
                    std::copy(&dope.rgbConfigData[0],   &dope.rgbConfigData[0]   + inc, reinterpret_cast<uint8_t*>(&mrdWidth));
                    std::copy(&dope.rgbConfigData[inc], &dope.rgbConfigData[inc] + inc, reinterpret_cast<uint8_t*>(&mrdHeight));
                    #if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
                        mrdWidth  = bswap_32(mrdWidth);
                        mrdHeight = bswap_32(mrdHeight);
                    #endif
                    op->setMaxResDecode(mrdWidth, mrdHeight);
                    NTRACE(TRACE_DRMSYSTEM, "MaxResDecode:\n\t"
                            "width : %d\n\t"
                            "height: %d",
                        op->maxResDecodeWidth,
                        op->maxResDecodeHeight);
                    break;
                }
            }
            break;
        }
        case DRM_EXTENDED_RESTRICTION_CONDITION_CALLBACK:
        case DRM_EXTENDED_RESTRICTION_ACTION_CALLBACK:
            // ignored
            break;
        case DRM_EXTENDED_RESTRICTION_QUERY_CALLBACK:
            // ignored
            /* Report that restriction was not understood */
            dr = DRM_E_EXTENDED_RESTRICTION_NOT_UNDERSTOOD;
            break;
        default:
            // ignored
            dr = DRM_E_NOTIMPL;
            break;
    }
    return dr;
}

#ifdef NRDP_HAS_TRACING
const char * licTypeStr(int licenseType)
{
    switch (licenseType)
    {
        default:
        case INVALID_LICENSE:          return "INVALID";  break;
        case LIMITED_DURATION_LICENSE: return "LDL";      break;
        case STANDARD_LICENSE:         return "STD"; break;
    }
}

const char * sesStateStr(int sessionState)
{
    switch (sessionState)
    {
        default:
        case IDrmSession::InvalidState:            return "InvalidState";            break;
        case IDrmSession::LicenseAcquisitionState: return "LicenseAcquisitionState"; break;
        case IDrmSession::InactiveDecryptionState: return "InactiveDecryptionState"; break;
        case IDrmSession::ActiveDecryptionState:   return "ActiveDecryptionState";   break;
    }
}
#endif  // NRDP_HAS_TRACING

const Vui8 zeros(DRM_ID_SIZE, 0);

void toggleKeyIdFormat(Vui8& keyId)
{
    assert(keyId.size() > 8);
    // Converting the KID format between the standard and PlayReady formats
    // consists of switching endian on bytes 0-3, 4-5, and 6-7.
    std::swap(keyId[0], keyId[3]);
    std::swap(keyId[1], keyId[2]);
    std::swap(keyId[4], keyId[5]);
    std::swap(keyId[6], keyId[7]);
}

} // namespace anonymous

// Trace macro to put function name, session id, and license type in automatically.
// Use only inside PlayReadyDrmSession member functions.
#define MYTRACE(fmt, ...) NTRACE(TRACE_DRMSYSTEM, "PlayReadyDr3mSession_%d(%s)::%s: " fmt, mSessionId, licTypeStr(mLicenseType), __func__, ##__VA_ARGS__)

OutputProtection::OutputProtection()
    : compressedDigitalVideoLevel(0)
    , uncompressedDigitalVideoLevel(0)
    , analogVideoLevel(0)
    , compressedDigitalAudioLevel(0)
    , uncompressedDigitalAudioLevel(0)
    , maxResDecodeWidth(0)
    , maxResDecodeHeight(0)
{}

void OutputProtection::setOutputLevels(const DRM_MINIMUM_OUTPUT_PROTECTION_LEVELS& opLevels)
{
    compressedDigitalVideoLevel   = opLevels.wCompressedDigitalVideo;
    uncompressedDigitalVideoLevel = opLevels.wUncompressedDigitalVideo;
    analogVideoLevel              = opLevels.wAnalogVideo;
    compressedDigitalAudioLevel   = opLevels.wCompressedDigitalAudio;
    uncompressedDigitalAudioLevel = opLevels.wUncompressedDigitalAudio;
}

void OutputProtection::setMaxResDecode(uint32_t width, uint32_t height)
{
    maxResDecodeWidth  = width;
    maxResDecodeHeight = height;
}

PlayReady3DrmSession::DecryptContext::DecryptContext()
{
    ZEROMEM(&drmDecryptContext, sizeof(DRM_DECRYPT_CONTEXT));
}

PlayReady3DrmSession::PlayReady3DrmSession(uint32_t sessionId,
                                         const std::string& contentId,
                                         enum DrmLicenseType licenseType,
                                         const std::vector<uint8_t>& drmHeader,
                                         std::shared_ptr<PlayReady3DrmSystem> drmSystem)
    : mSessionState(InvalidState)
    , mSessionId(sessionId)
    , mContentId(contentId)
    , mLicenseType(licenseType)
    , mDrmHeader(drmHeader)
    , mDrmSystem(drmSystem)
    , mAppContext(mDrmSystem->getDrmAppContext())
    , mDrmDecryptContextMutex(DRMCONTEXT_MUTEX, "PlayReady3DrmSession::mDrmDecryptContextMutex")
{
    MYTRACE("contentId %s, drmHeader.size() %zu",
            Base64::encode(contentId).c_str(), drmHeader.size());
    //MYTRACE("drmHeader = \n%s", vectorToBase64String(drmHeader).c_str());
}

PlayReady3DrmSession::~PlayReady3DrmSession()
{
    MYTRACE();
    finalizeDrmSession();
    mDrmHeader.clear();
}

void PlayReady3DrmSession::finalizeDrmSession()
{
    ScopedMutex decryptContextLock(mDrmDecryptContextMutex);
    ScopedMutex systemLock(mDrmSystem->getDrmAppContextMutex());
    MYTRACE();
    cleanupDecryptContext();
    setSessionState(IDrmSession::InvalidState);
    clearLicense();
}

void PlayReady3DrmSession::cleanupDecryptContext()
{
    ScopedMutex decryptContextLock(mDrmDecryptContextMutex);
    // Close all decryptors that were created on this session
    for (DecryptContextMap::iterator it = mDecryptContextMap.begin(); it != mDecryptContextMap.end(); ++it)
    {
        MYTRACE("Drm_Reader_Close for keyId %s", vectorToBase64String(it->first).c_str());
        Drm_Reader_Close(&(it->second->drmDecryptContext));
    }
    mDecryptContextMap.clear();
}

NFErr PlayReady3DrmSession::clearLicense()
{
    ScopedMutex systemLock(mDrmSystem->getDrmAppContextMutex());

    MYTRACE();

    if (mBatchId.empty()) {
        return NFErr_OK;
    }

    // Delete all licenses associated with this session.
    MYTRACE("Clear licenses for BID %s", vectorToBase64String(mBatchId).c_str());
    DRM_ID batchDrmId;
    vectorIdToDrmId(mBatchId, batchDrmId);
    MYTRACE("Drm_StoreMgmt_DeleteInMemoryLicenses");
    DRM_RESULT result = Drm_StoreMgmt_DeleteInMemoryLicenses(mAppContext.get(), &batchDrmId);
    // Since there are multiple licenses in a batch, we might have already cleared
    // them all. Ignore DRM_E_NOMORE returned from Drm_StoreMgmt_DeleteInMemoryLicenses.
    if (DRM_FAILED(result) && (result != DRM_E_NOMORE)) {
        return NfError(result, DRMSYSTEM_LICENSE_ERROR, "Error in Drm_StoreMgmt_DeleteInMemoryLicenses");
    }

    return NFErr_OK;
}

std::string PlayReady3DrmSession::getDrmType()
{
    return DrmSystem::type;
}

void PlayReady3DrmSession::setSessionState(enum IDrmSession::SessionState sessionState)
{
    MYTRACE("from %s to %s", sesStateStr(mSessionState), sesStateStr(sessionState));
    mSessionState = sessionState;
}

NFErr PlayReady3DrmSession::getChallengeData(std::vector<uint8_t>& challenge, bool isLDL)
{
    NRDP_UNUSED(isLDL);

    MYTRACE("challenge=%p", &challenge);

    // open scope for DRM_APP_CONTEXT mutex
    ScopedMutex systemLock(mDrmSystem->getDrmAppContextMutex());

    // sanity check for drm header
    if (mDrmHeader.size() == 0) {
        return NFErr_NotAllowed;
    }

    // Set this session's DMR header in the PR3 app context.
    NFErr nfError = setDrmHeader();
    if (nfError != NFErr_OK)
        return nfError;

    // Find the size of the challenge.
    MYTRACE("Drm_LicenseAcq_GenerateChallenge, querying challenge size");
    DRM_DWORD challengeSize = 0;
    DRM_RESULT err;
    err = Drm_LicenseAcq_GenerateChallenge(
            mAppContext.get(),
            RIGHTS,
            sizeof(RIGHTS) / sizeof(DRM_CONST_STRING*),
            NULL,
            NULL, 0,
            NULL, NULL,
            NULL, NULL,
            NULL, &challengeSize);
    if (err != DRM_E_BUFFERTOOSMALL) {
        return NfError(err, DRMSYSTEM_CHALLENGE_ERROR, "Error in Drm_LicenseAcq_GenerateChallenge");
    }

    // Now get the challenge.
    MYTRACE("Drm_LicenseAcq_GenerateChallenge: making challenge, size=%u",
           static_cast<uint32_t>(challengeSize));
    challenge.resize(challengeSize);
    err = Drm_LicenseAcq_GenerateChallenge(
            mAppContext.get(),
            RIGHTS,
            sizeof(RIGHTS) / sizeof(DRM_CONST_STRING*),
            NULL,
            NULL, 0,
            NULL, NULL,
            NULL, NULL,
            (DRM_BYTE*)&challenge[0],
            &challengeSize);
    if (DRM_FAILED(err)) {
        return NfError(err, DRMSYSTEM_CHALLENGE_ERROR, "Error in Drm_LicenseAcq_GenerateChallenge");
    }
    // Do a final resize of the challenge bufffer, as the final size might be
    // (and often is) smaller than the allocation size reported in the first
    // call to Drm_LicenseAcq_GenerateChallenge().
    challenge.resize(challengeSize);

    //const std::string chalStr(challenge.begin(), challenge.end());
    //MYTRACE("\n%s", chalStr.c_str());

    return NFErr_OK;
}

NFErr PlayReady3DrmSession::storeLicenseData(const std::vector<uint8_t>& licenseData,
                                            std::vector<unsigned char> &secureStopId)
{
    // open scope for DRM_APP_CONTEXT mutex
    ScopedMutex systemLock(mDrmSystem->getDrmAppContextMutex());

    //const std::string licStr(licenseData.begin(), licenseData.end());
    //MYTRACE("\n%s", licStr.c_str());

    // === Store the license and check for license processing errors.
    //
    // NOTE: Drm_LicenseAcq_ProcessResponse() is the PR3 API that creates a
    // secure stop. There is one secure stop per license response, regardless of
    // how many licenses are in the response data.
    //

    MYTRACE("Drm_LicenseAcq_ProcessResponse");
    DRM_LICENSE_RESPONSE drmLicenseResponse;
    // MUST zero the input DRM_LICENSE_RESPONSE struct!
    ZEROMEM(&drmLicenseResponse, sizeof(DRM_LICENSE_RESPONSE));
    DRM_RESULT err;
    // Non-persistent licenses (the kind in use) have no signature, so the
    // LIC_RESPONSE_SIGNATURE_NOT_REQUIRED flag must be used.
    err = Drm_LicenseAcq_ProcessResponse(
            mAppContext.get(),
            DRM_PROCESS_LIC_RESPONSE_SIGNATURE_NOT_REQUIRED,
            &licenseData[0],
            (DRM_DWORD)licenseData.size(),
            &drmLicenseResponse);

    // First, check the return code of Drm_LicenseAcq_ProcessResponse()
    if (err ==  DRM_E_LICACQ_TOO_MANY_LICENSES) {
        // This means the server response contained more licenses than
        // DRM_MAX_LICENSE_ACK (usually 20). Should allocate space and retry.
        // FIXME NRDLIB-4481: This will need to be implemented when we start
        // using batch license requests.
        return NfError(err, DRMSYSTEM_LICENSE_ERROR, "Drm_LicenseAcq_ProcessResponse too many licenses in response");
    }
    else if (DRM_FAILED(err)) {
        return NfError(err, DRMSYSTEM_LICENSE_ERROR, "Error in Drm_LicenseAcq_ProcessResponse");
    }

    // Next, examine the returned drmLicenseResponse struct for a top-level error.
    if (DRM_FAILED(drmLicenseResponse.m_dwResult)) {
        return NfError(drmLicenseResponse.m_dwResult, DRMSYSTEM_LICENSE_ERROR, "Error in DRM_LICENSE_RESPONSE");
    }

    // Finally, ensure that each license in the response was processed
    // successfully.
    const DRM_DWORD nLicenses = drmLicenseResponse.m_cAcks;
    for (uint32_t i=0; i < nLicenses; ++i)
    {
        MYTRACE("Checking license %d", i);
        if (DRM_FAILED(drmLicenseResponse.m_rgoAcks[i].m_dwResult)) {
            // Special handling for DRM_E_DST_STORE_FULL. If this error is
            // detected for any license, reset the DRM appcontext and return error.
            if (drmLicenseResponse.m_rgoAcks[i].m_dwResult == DRM_E_DST_STORE_FULL) {
                MYTRACE("Found DRM_E_DST_STORE_FULL error in license %d, reinitializing DrmSystem!", i);
                Log::error(TRACE_DRMSYSTEM, "%s: Found DRM_E_DST_STORE_FULL error in license %d, reinitializing DrmSystem!", methodName(__PRETTY_FUNCTION__).c_str(), i);
                NFErr nfErr = mDrmSystem->reinitialize();
                if (!nfErr.ok()) {
                    MYTRACE("DrmSystem reinitialize failed");
                    return nfErr;
                }
            }
            else {
                MYTRACE("Error 0x%08lx found in license %d", (unsigned long)drmLicenseResponse.m_rgoAcks[i].m_dwResult, i);
            }
            return NfError(drmLicenseResponse.m_rgoAcks[i].m_dwResult,
                    DRMSYSTEM_LICENSE_ERROR, "Error in individual license");
        }
    }

    // === Extract various ID's from drmLicenseResponse
    //
    // There are 3 ID's in the processed license response we are interested in:
    // BID - License batch ID. A GUID that uniquely identifies a batch of
    //       licenses that were processed in one challenge/response transaction.
    //       The BID is a nonce unique to the transaction. If the transaction
    //       contains a single license, this is identical to the license nonce.
    //       The secure stop ID is set to the BID value.
    // KID - Key ID. A GUID that uniquely identifies the media content key. This
    //       is the primary index for items in the license store. There can be
    //       multiple licenses with the same KID.
    // LID - License ID. A GUID that uniquely identifies a license. This is the
    //       secondary index for items in the license store.
    // When there are multiple licenses in the server response as in the PRK
    // case, there are correspondingly multiple KID/LID entries in the processed
    // response. There is always only a single BID per server response.

    // BID
    mBatchId = drmIdToVectorId(&drmLicenseResponse.m_oBatchID);
    MYTRACE("BID: %s", vectorToBase64String(mBatchId).c_str());
    // Microsoft says that a batch ID of all zeros indicates some sort of error
    // for in-memory licenses. Hopefully this error was already caught above.
    if (std::equal(mBatchId.begin(), mBatchId.end(), zeros.begin())) {
        return NfError(-1, DRMSYSTEM_LICENSE_ERROR, "0 batch ID in processed response");
    }
    // We take the batch ID as the secure stop ID
    secureStopId = mBatchId;
    MYTRACE("SSID: %s", vectorToBase64String(mBatchId).c_str());

    // KID and LID
    mLicenseIds.clear();
    mKeyIds.clear();
    MYTRACE("Found %d license%s in server response:", nLicenses, (nLicenses > 1) ? "s" : "");
    for (uint32_t i=0; i < nLicenses; ++i)
    {
        const DRM_LICENSE_ACK * const licAck = &drmLicenseResponse.m_rgoAcks[i];
        mLicenseIds.push_back(drmIdToVectorId(&licAck->m_oLID));
        mKeyIds.push_back    (drmIdToVectorId(&licAck->m_oKID));
        MYTRACE("KID[%d]:  %s", i, vectorToBase64String(mKeyIds[i]).c_str());
    }

#ifdef KIDFLIP_DEBUG
    Log::warn(TRACE_DRMSYSTEM, "==== PlayReady3DrmSystem %3d STOR: KID %s", mSessionId, vectorToBase64String(mKeyIds[0]).c_str());
#endif

    return NFErr_OK;
}

NFErr PlayReady3DrmSession::initDecryptContext()
{
    ScopedMutex systemLock(mDrmSystem->getDrmAppContextMutex());
    // Choose the first KID. In the case where this session owns only a single
    // key, this does the right thing. In the PRK case, the other overload of
    // this method should be called to select from the set of licenses known to
    // this session.
    if (mKeyIds.size() == 0) {
        return NfError(-1, DRMSYSTEM_LICENSE_ERROR, "No license on this session");
    } else if (mKeyIds.size() != 1) {
        return NfError(-1, DRMSYSTEM_LICENSE_ERROR, "More that one license on this session");
    }
    return initDecryptContextPrivate(mKeyIds[0]);
}

// If DONT_ACCEPT_BOTH_KID_FORMATS is defined, a keyId in only the standard TENC
// format is accepted. Otherwise keyId may be in either the standard TENC or the
// PlayReady formats.
NFErr PlayReady3DrmSession::initDecryptContextByKid(const std::string& keyId)
{
    ScopedMutex systemLock(mDrmSystem->getDrmAppContextMutex());
    Vui8 keyIdVec(keyId.begin(), keyId.end());
    toggleKeyIdFormat(keyIdVec);  // switch from TENC to PlayReady format

#ifdef KIDFLIP_DEBUG
    Log::warn(TRACE_DRMSYSTEM, "==== PlayReady3DrmSystem %3d INDC: KID %s", mSessionId, Base64::encode(keyId).c_str());
#endif

    // === Ensure the license specified by the keyId is known to this session.
#ifdef DONT_ACCEPT_BOTH_KID_FORMATS
    if (!hasLicensePrivate(keyIdVec))
        return NfError(-1, DRMSYSTEM_LICENSE_ERROR, "KeyId not known to this session");
#else
    bool found = hasLicensePrivate(keyIdVec);
    if (!found) {  // might be wrong format, switch back and retry
        toggleKeyIdFormat(keyIdVec);
        found = hasLicensePrivate(keyIdVec);
        if (found) {
            MYTRACE("Corrected incoming swapped KID format");
        }
    }
    if (!found)
        return NfError(-1, DRMSYSTEM_LICENSE_ERROR, "KeyId not known to this session");
#endif

    MYTRACE("KID %s", vectorToBase64String(keyIdVec).c_str());

    return initDecryptContextPrivate(keyIdVec);
}

NFErr PlayReady3DrmSession::initDecryptContextPrivate(const Vui8& keyId)
{
    // === Find existing decryption context for this keyId, or create a new one.
    DecryptContextMap::iterator pdc = mDecryptContextMap.find(keyId);
    if (pdc != mDecryptContextMap.end()) {
        // We already have a decryption context for this keyId. Make it active.
        MYTRACE("Found existing decrypt context for keyId %s",
                vectorToBase64String(pdc->first).c_str());
        mActiveDecryptContext = pdc->second;
    }
    else {
        // We need to create a new decryption context.

        // Make the current app context contain the DRM header for this session.
        NFErr nfError = setDrmHeader();
        if (nfError != NFErr_OK)
            return nfError;

        // Select the license in the current DRM header by keyId
        nfError = setKeyId(keyId);
        if (nfError != NFErr_OK)
            return nfError;

        // Create a decryption context and bind it to the license specified
        // by the input keyId. This license must already be in the PlayReady
        // license store.
        std::shared_ptr<DecryptContext> newDecryptContext(new DecryptContext);
        MYTRACE("Drm_Reader_Bind");
        DRM_RESULT err = Drm_Reader_Bind(
                mAppContext.get(),
                RIGHTS,
                sizeof(RIGHTS) / sizeof(DRM_CONST_STRING*),
                &bindCallback,
                &(newDecryptContext->outputProtection),
                &(newDecryptContext->drmDecryptContext));
        if (DRM_FAILED(err)) {
            return NfError(err, DRMSYSTEM_LICENSE_ERROR, "Error in Drm_Reader_Bind");
        }
        MYTRACE("Created decryption context for keyId %s",
                vectorToBase64String(keyId).c_str());

        // Commit all secure store transactions to the DRM store file. For the
        // Netflix use case, Drm_Reader_Commit only needs to be called after
        // Drm_Reader_Bind.
        MYTRACE("Drm_Reader_Commit");
        err = Drm_Reader_Commit(mAppContext.get(), NULL, NULL);
        if (DRM_FAILED(err)) {
            return NfError(err, DRMSYSTEM_LICENSE_ERROR, "Error in Drm_Reader_Commit");
        }

        // Save the new decryption context to our member map, and make it the
        // active one.
        mDecryptContextMap.insert(std::make_pair(keyId, newDecryptContext));
        mActiveDecryptContext = newDecryptContext;
    }

    return NFErr_OK;
}

NFErr PlayReady3DrmSession::decrypt(unsigned char* IVData,
                                   uint32_t IVDataSize,
                                   ullong byteOffset,
                                   DataSegment& dataSegment)
{
    // open scope for DRM_APP_CONTEXT mutex
    ScopedMutex systemLock(mDrmSystem->getDrmAppContextMutex());

    NRDP_UNUSED(IVDataSize);
    assert(IVDataSize > 0);
    DRM_BYTE* data = (DRM_BYTE*)dataSegment.data;
    uint32_t dataSize = dataSegment.numBytes;
    if(dataSize == 0){
        return NFErr_OK;
    }

    //MYTRACE("data=%p length=%d", data, dataSize);

    if (!mActiveDecryptContext) {
        return NfError(-1, DECRYPTION_ERROR, "No active decryption context");
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
    //       IVDataSize, byteOffset, ctrContext.qwBlockOffset, ctrContext.bByteOffset, dataSize);
    DRM_RESULT err = Drm_Reader_DecryptLegacy(
            &(mActiveDecryptContext->drmDecryptContext),
            &ctrContext,
            data,
            dataSize);
    if (DRM_FAILED(err)) {
        MYTRACE("Drm_Reader_DecryptLegacy");
        return NfError(err, DECRYPTION_ERROR, "Error in Drm_Reader_Decrypt");
    }

    return NFErr_OK;
}

// If DONT_ACCEPT_BOTH_KID_FORMATS is defined, a keyId in only the standard TENC
// format is accepted. Otherwise keyId may be in either the standard TENC or the
// PlayReady formats.
bool PlayReady3DrmSession::hasLicense(const std::string& keyId)
{
    ScopedMutex systemLock(mDrmSystem->getDrmAppContextMutex());
    Vui8 keyIdVec(keyId.begin(), keyId.end());
    toggleKeyIdFormat(keyIdVec);
#ifdef DONT_ACCEPT_BOTH_KID_FORMATS
    return hasLicensePrivate(keyIdVec);
#else
    bool found = hasLicensePrivate(keyIdVec);
    if (!found) {  // might be wrong format, switch back and retry
        toggleKeyIdFormat(keyIdVec);
        found = hasLicensePrivate(keyIdVec);
    }
    return found;
#endif
}

bool PlayReady3DrmSession::hasLicensePrivate(const Vui8& keyId)
{
    return std::find(mKeyIds.begin(), mKeyIds.end(), keyId) != mKeyIds.end();
}

Mutex& PlayReady3DrmSession::getDecryptContextMutex()
{
    return mDrmDecryptContextMutex;
}

NFErr PlayReady3DrmSession::setDrmHeader()
{
    MYTRACE("Drm_Content_SetProperty DRM_CSP_AUTODETECT_HEADER");
    DRM_RESULT err = Drm_Content_SetProperty(
            mAppContext.get(),
            DRM_CSP_AUTODETECT_HEADER,
            &mDrmHeader[0],
            mDrmHeader.size());
    if (DRM_FAILED(err)) {
        return NfError(err, DRMSYSTEM_LICENSE_ERROR, "Error in Drm_Content_SetProperty");
    }
    return NFErr_OK;
}

NFErr PlayReady3DrmSession::setKeyId(const Vui8& keyId)
{
    assert(keyId.size() == DRM_ID_SIZE);

    // To use the DRM_CSP_SELECT_KID feature of Drm_Content_SetProperty(), the
    // KID must be base64-encoded for some reason.
    DRM_WCHAR rgwchEncodedKid[CCH_BASE64_EQUIV(DRM_ID_SIZE)]= {0};
    DRM_DWORD cchEncodedKid = CCH_BASE64_EQUIV(DRM_ID_SIZE);
    DRM_RESULT err = DRM_B64_EncodeW(&keyId[0], sizeof(DRM_KID), rgwchEncodedKid, &cchEncodedKid, 0);
    if (DRM_FAILED(err)) {
        return NfError(err, DRMSYSTEM_LICENSE_ERROR, "Error base64-encoding KID");
    }

    MYTRACE("Drm_Content_SetProperty DRM_CSP_SELECT_KID");
    err = Drm_Content_SetProperty(
            mAppContext.get(),
            DRM_CSP_SELECT_KID,
            (DRM_BYTE*)rgwchEncodedKid,
            sizeof(rgwchEncodedKid));
    if (DRM_FAILED(err)) {
        return NfError(err, DRMSYSTEM_LICENSE_ERROR, "Error in Drm_Content_SetProperty");
    }
    return NFErr_OK;
}

uint32_t PlayReady3DrmSession::maxResDecodeWidth() const
{
    if (!mActiveDecryptContext)
        return 0;
    return mActiveDecryptContext->outputProtection.maxResDecodeWidth;
}

uint32_t PlayReady3DrmSession::maxResDecodeHeight() const
{
    if (!mActiveDecryptContext)
        return 0;
    return mActiveDecryptContext->outputProtection.maxResDecodeHeight;
}

uint16_t PlayReady3DrmSession::compressedDigitalVideoLevel() const
{
    if (!mActiveDecryptContext)
        return 0;
    return mActiveDecryptContext->outputProtection.compressedDigitalVideoLevel;
}

uint16_t PlayReady3DrmSession::uncompressedDigitalVideoLevel() const
{
    if (!mActiveDecryptContext)
        return 0;
    return mActiveDecryptContext->outputProtection.uncompressedDigitalVideoLevel;
}

uint16_t PlayReady3DrmSession::analogVideoLevel() const
{
    if (!mActiveDecryptContext)
        return 0;
    return mActiveDecryptContext->outputProtection.analogVideoLevel;
}

uint16_t PlayReady3DrmSession::compressedDigitalAudioLevel() const
{
    if (!mActiveDecryptContext)
        return 0;
    return mActiveDecryptContext->outputProtection.compressedDigitalAudioLevel;
}

uint16_t PlayReady3DrmSession::uncompressedDigitalAudioLevel() const
{
    if (!mActiveDecryptContext)
        return 0;
    return mActiveDecryptContext->outputProtection.uncompressedDigitalAudioLevel;
}

#if defined(DUMPING_BIND_CALLBACK)

namespace { // anonymous

std::string toJson(DRM_DWORD x, size_t width /* = 0*/)
{
    // If a width is specified, then it is assumed that we want see a hex value showing that many bytes
    std::ostringstream ss;
    if (width > 0) {
        ss << "\"0x";
        ss << std::hex;
        ss.width(width * 2);
        ss.fill('0');
    }
    ss << x;
    if (width > 0) {
        ss << '\"';
    }
    return ss.str();
}

std::string toJson(DRM_VOID * x)
{
    std::ostringstream ss;
    ss << "\"0x";
    ss << std::hex;
    ss << std::right;
    ss.fill('0');
    ss.width(sizeof(x) * 2);
    ss << uintptr_t(x);
    ss << '\"';
    return ss.str();
}

std::string toJson(DRM_GUID const & x)
{
    std::ostringstream ss;
    ss << std::hex;
    ss << std::right;
    ss.fill('0');
    ss << '\"';
    ss.width(8);
    ss.fill('0');
    ss << x.Data1;
    ss << '-';
    ss.width(4);
    ss.fill('0');
    ss << x.Data2;
    ss << '-';
    ss.width(4);
    ss.fill('0');
    ss << x.Data3;
    ss << '-';
    ss.width(2);
    ss.fill('0');
    ss << (int)x.Data4[0];
    ss.width(2);
    ss.fill('0');
    ss << (int)x.Data4[1];
    ss << '-';
    ss.width(2);
    ss.fill('0');
    ss << (int)x.Data4[2];
    ss.width(2);
    ss.fill('0');
    ss << (int)x.Data4[3];
    ss.width(2);
    ss.fill('0');
    ss << (int)x.Data4[4];
    ss.width(2);
    ss.fill('0');
    ss << (int)x.Data4[5];
    ss.width(2);
    ss.fill('0');
    ss << (int)x.Data4[6];
    ss.width(2);
    ss.fill('0');
    ss << (int)x.Data4[7];
    ss << '\"';
    return ss.str();
}

std::string toJson(DRM_OUTPUT_PROTECTION_EX const & x)
{
    std::ostringstream ss;
    ss << '{';
    ss << "\"dwVersion\":" << toJson(x.dwVersion) << ',';
    ss << "\"guidId\":" << toJson(x.guidId) << ',';
    ss << "\"rgbConfigData[0:7]\":" << toJson(8, x.rgbConfigData);
    ss << '}';
    return ss.str();
}

std::string toJson(DRM_XB_UNKNOWN_OBJECT const & x)
{
    (void)x;
    return "\"...\"";
}

std::string toJson(DRM_XMRFORMAT const & x)
{
    (void)x;
    return "\"...\"";
}

std::string toJson(DRM_MINIMUM_OUTPUT_PROTECTION_LEVELS const & x)
{
    std::ostringstream ss;
    ss << '{';
    ss << "\"wCompressedDigitalVideo\":" << toJson(x.wCompressedDigitalVideo) << ',';
    ss << "\"wUncompressedDigitalVideo\":" << toJson(x.wUncompressedDigitalVideo) << ',';
    ss << "\"wAnalogVideo\":" << toJson(x.wAnalogVideo) << ',';
    ss << "\"wCompressedDigitalAudio\":" << toJson(x.wCompressedDigitalAudio) << ',';
    ss << "\"wUncompressedDigitalAudio\":" << toJson(x.wUncompressedDigitalAudio);
    ss << '}';
    return ss.str();
}

std::string toJson(DRM_OPL_OUTPUT_IDS const & x)
{
    std::ostringstream ss;
    ss << '{';
    ss << "\"rgIds\":" << toJson(x.cIds, x.rgIds);
    ss << '}';
    return ss.str();
}

std::string toJson(DRM_VIDEO_OUTPUT_PROTECTION_IDS_EX const & x)
{
    std::ostringstream ss;
    ss << '{';
    ss << "\"dwVersion\":" << toJson(x.dwVersion) << ',';
    ss << "\"rgVop\":" << toJson(x.cEntries, x.rgVop);
    ss << '}';
    return ss.str();
}

std::string toJson(DRM_AUDIO_OUTPUT_PROTECTION_IDS_EX const & x)
{
    std::ostringstream ss;
    ss << '{';
    ss << "\"dwVersion\":" << toJson(x.dwVersion) << ',';
    ss << "\"rgAop\":" << toJson(x.cEntries, x.rgAop);
    ss << '}';
    return ss.str();
}

std::string toJson(DRM_PLAY_OPL_EX2 const & opl)
{
    std::ostringstream ss;
    ss << '{';
    ss << "\"dwVersion\":" << toJson(opl.dwVersion) << ',';
    ss << "\"minOPL\":" << toJson(opl.minOPL) << ',';
    ss << "\"oplIdReserved\":" << toJson(opl.oplIdReserved) << ',';
    ss << "\"vopi\":" << toJson(opl.vopi) << ',';
    ss << "\"aopi\":" << toJson(opl.aopi) << ',';
    ss << "\"dvopi\":" << toJson(opl.dvopi);
    ss << '}';
    return ss.str();
}

std::string toJson(DRM_EXTENDED_RESTRICTION_CALLBACK_STRUCT const & r)
{
    std::ostringstream ss;
    ss << '{';
    ss << "\"wRightID\":" << toJson(r.wRightID) << ',';
    ss << "\"pRestriction\":" << (r.pRestriction ? toJson(*r.pRestriction) : toJson((void *)0)) << ',';
    ss << "\"pXMRLicense\":" << (r.pXMRLicense ? toJson(*r.pXMRLicense) : toJson((void *)0)) << ',';
    ss << "\"pContextSST\":" << toJson(r.pContextSST);
    ss << '}';
    return ss.str();
}

void dumpBindCallback(DRM_VOID const * cb, DRM_POLICY_CALLBACK_TYPE type)
{
    char const * typeName;
    std::string json;
    switch (type) {
        case DRM_PLAY_OPL_CALLBACK:
        {
            DRM_PLAY_OPL_EX2 const * opl = static_cast<DRM_PLAY_OPL_EX2 const *>(cb);
            typeName = "PLAY_OPL";
            json = toJson(*opl);
            break;
        }
        case DRM_EXTENDED_RESTRICTION_CONDITION_CALLBACK:
        {
            DRM_EXTENDED_RESTRICTION_CALLBACK_STRUCT const * r = static_cast<DRM_EXTENDED_RESTRICTION_CALLBACK_STRUCT const *>(cb);
            typeName = "EXTENDED_RESTRICTION_CONDITION";
            json = toJson(*r);
            break;
        }
        case DRM_EXTENDED_RESTRICTION_ACTION_CALLBACK:
        {
            DRM_EXTENDED_RESTRICTION_CALLBACK_STRUCT const * r = static_cast<DRM_EXTENDED_RESTRICTION_CALLBACK_STRUCT const *>(cb);
            typeName = "EXTENDED_RESTRICTION_ACTION";
            json = toJson(*r);
            break;
        }
        case DRM_EXTENDED_RESTRICTION_QUERY_CALLBACK:
        {
            DRM_EXTENDED_RESTRICTION_CALLBACK_STRUCT const * r = static_cast<DRM_EXTENDED_RESTRICTION_CALLBACK_STRUCT const *>(cb);
            typeName = "EXTENDED_RESTRICTION_QUERY";
            json = toJson(*r);
            break;
        }
        default:
            typeName = "(unknown)";
            json = "{}";
            break;
    }
    NTRACE(TRACE_DRMSYSTEM, "bindCallback: %s: %s", typeName, json.c_str());
}

} // namespace anonymous

#endif // if defined(DUMPING_BIND_CALLBACK)

}}  // namespace netflix::device
