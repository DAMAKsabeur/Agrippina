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

#include "PlayReady3DrmSystem.h"

#include <stdio.h>
#include <errno.h>

#include <nrdbase/Configuration.h>
#include <nrdbase/ReadDir.h>
#include <nrd/AppLog.h>
#include <nrdbase/Base64.h>
#include <nrd/IDeviceError.h>
#include <nrd/IDrmSession.h>

#include "PlayReady3DrmSession.h"
#include "PlayReady3MeteringCert.h"

#include <drmversionconstants.h>

#include <openssl/sha.h>

extern DRM_CONST_STRING g_dstrDrmPath;

namespace netflix {
namespace device {

namespace { // anonymous

typedef std::vector<unsigned char> Vuc;

// The following two values determine the initial size of the in-memory license
// store. If more licenses are used concurrently, Playready will resize the
// to make room. However, the resizing action is inefficient in both CPU and
// memory, so it is useful to get the max size right and set it here.
const DRM_DWORD LICENSE_SIZE_BYTES = 512;  // max possible license size (ask the server team)
const DRM_DWORD MAX_NUM_LICENSES = 200;    // max number of licenses (ask the RefApp team)

// Each challenge saves a nonce to the PlayReady3 nonce store, and each license
// bind removes a nonce. The nonce store is also a FIFO, with the oldest nonce
// rolling off if the store is full when a new challenge is generated. This can
// be a problem if the client generates but does not process a number of licenses
// greater than the nonce fifo. So NONCE_STORE_SIZE is reported to the client
// via the getLdlSessionLimit() API.
const uint32_t NONCE_STORE_SIZE = 100;

// Creates a new DRM_WCHAR[] on the heap from the provided string.
// Note: Caller takes ownership of returned heap memory.
DRM_WCHAR* createDrmWchar(std::string const& s)
{
    DRM_WCHAR* w = new DRM_WCHAR[s.length() + 1];
    for (size_t i = 0; i < s.length(); ++i)
        w[i] = DRM_ONE_WCHAR(s[i], '\0');
    w[s.length()] = DRM_ONE_WCHAR('\0', '\0');
    return w;
}

class DrmSecureStop : public netflix::device::ISecureStop
{
private:
    const Vuc m_sessionID;
    mutable Vuc m_rawData;
public:
    DrmSecureStop(const Vuc &sessionID, size_t dataSize)
        : m_sessionID(sessionID), m_rawData(dataSize, 0) {}
    DrmSecureStop(const Vuc &sessionID, const unsigned char *data, size_t dataSize)
        : m_sessionID(sessionID), m_rawData(dataSize, 0)
    {
        m_rawData.reserve(dataSize);
        std::copy(data, data + dataSize, m_rawData.begin());
    }
    virtual ~DrmSecureStop() {}
    virtual unsigned char *getRawData() {return &m_rawData[0];}
    virtual size_t getRawDataSize() {return m_rawData.size();}
    virtual void getSessionID(Vuc &sessionID) {sessionID = m_sessionID;}
};

bool calcFileSha256 (const std::string& filePath, Vuc& hash)
{
    FILE* const file = fopen(filePath.c_str(), "rb");
    if (!file)
        return false;
    SHA256_CTX sha256;
    SHA256_Init(&sha256);
    const int BUFSIZE = 32768;
    Vuc buffer(BUFSIZE, 0);
    size_t bytesRead = 0;
    while ((bytesRead = fread(&buffer[0], 1, BUFSIZE, file)))
        SHA256_Update(&sha256, &buffer[0], bytesRead);
    fclose(file);
    hash.resize(SHA256_DIGEST_LENGTH, 0);
    SHA256_Final(&hash[0], &sha256);
    return true;
}

// copied from Playready oem/linux/oemfileio.c
/**********************************************************************
** Function:    PackedCharsToNative
** Synopsis:    Convert packed string to native byte string
** Arguments:   [f_pPackedString] -- packed string,
**                                   size of the string should be sufficient for conversion to native byte string
**              [f_cch]           -- size of input string in elements.
**                                   The NULL terminator should be included in the count if it is to be copied
***********************************************************************/
void PackedCharsToNative(
    __inout_ecount(f_cch) DRM_CHAR *f_pPackedString,
                          DRM_DWORD f_cch )
{
    DRM_DWORD ich = 0;

    if( f_pPackedString == NULL
     || f_cch == 0 )
    {
        return;
    }
    for( ich = 1; ich <= f_cch; ich++ )
    {
        f_pPackedString[f_cch - ich] = ((DRM_BYTE*)f_pPackedString)[ f_cch - ich ];
    }
}

} // namespace anonymous

// Trace macro for use only inside PlayReadyDrmSystem member functions.
#define MYTRACE(fmt, ...) NTRACE(TRACE_DRMSYSTEM, "%s: " fmt, methodName(__PRETTY_FUNCTION__).c_str(), ##__VA_ARGS__);

using namespace esplayer;

const std::string DrmSystem::type = "PlayReady";

PlayReady3DrmSystem::PlayReady3DrmSystem()
    : sessionId_(0)
    , drmdir_(NULL)
    , drmAppContextMutex_(DRMAPPCONTEXT_MUTEX, "PlayReady3DrmSystem::drmAppContextMutex")
    , appContextOpaqueBuffer_(new DRM_BYTE[MINIMUM_APPCONTEXT_OPAQUE_BUFFER_SIZE])
    , pbRevocationBuffer_(new DRM_BYTE[REVOCATION_BUFFER_SIZE])
{
    MYTRACE();

    assert(appContextOpaqueBuffer_);
    assert(pbRevocationBuffer_);

    const std::string rdir = Configuration::dataReadPath() + "/dpi/playready";
    const std::string wdir = Configuration::dataWritePath() + "/dpi/playready/storage/";
    ReadDir::recursiveMakeDirectory(wdir);
    drmStorePath_ = wdir + "drmstore";

    // Create wchar strings from the arguments.
    drmdir_ = createDrmWchar(rdir);

    // Initialize PlayReady directory.
    g_dstrDrmPath.pwszString = drmdir_;
    g_dstrDrmPath.cchString = rdir.length();

    // Save the DRM store location for later.
    drmStore_.pwszString = createDrmWchar(drmStorePath_);
    drmStore_.cchString = drmStorePath_.length();
}

PlayReady3DrmSystem::~PlayReady3DrmSystem()
{
    ScopedMutex lock(drmAppContextMutex_);
    if (appContext_.get())
    {
        Drm_Reader_Commit(appContext_.get(), NULL, NULL);
        Drm_Uninitialize(appContext_.get());
        appContext_.reset();
    }
    delete [] drmdir_;
    delete [] drmStore_.pwszString;
    delete [] appContextOpaqueBuffer_;
    delete [] pbRevocationBuffer_;
}

NFErr PlayReady3DrmSystem::initialize()
{
    ScopedMutex lock(drmAppContextMutex_);

    MYTRACE();

    // Log::warn instead of NTRACE to get the PlayReady version message into the official log
    Log::warn(TRACE_DRMSYSTEM, "%s: " "%s %s", methodName(__PRETTY_FUNCTION__).c_str(), DrmSystem::type.c_str(), getDrmVersion().c_str());

    // Platform initialization
    DRM_RESULT err = Drm_Platform_Initialize(NULL);
    if (DRM_FAILED(err)) {
        return NfError(err, DRMSYSTEM_INITIALIZATION_ERROR, "Error in Drm_Platform_Initialize");
    }

    return reinitialize();
}

NFErr PlayReady3DrmSystem::reinitialize()
{
    ScopedMutex lock(drmAppContextMutex_);

    MYTRACE();

    // Application context initialization
    appContext_.reset(new DRM_APP_CONTEXT);
    memset(appContext_.get(), 0, sizeof(DRM_APP_CONTEXT));
    DRM_RESULT err = Drm_Initialize(
            appContext_.get(),
            NULL,
            appContextOpaqueBuffer_,
            MINIMUM_APPCONTEXT_OPAQUE_BUFFER_SIZE,
            &drmStore_);
    if (DRM_FAILED(err)) {
        return NfError(err, DRMSYSTEM_INITIALIZATION_ERROR, "Error in Drm_Initialize");
    }

    // Specify the initial size of the in-memory license store. The store will
    // grow above this size if required during usage, using a memory-doubling
    // algorithm. So it is more efficient, but not required, to get the size
    // correct from the beginning.
    err = Drm_ResizeInMemoryLicenseStore(appContext_.get(), MAX_NUM_LICENSES * LICENSE_SIZE_BYTES);
    if (DRM_FAILED(err)) {
        return NfError(err, DRMSYSTEM_INITIALIZATION_ERROR, "Error in Drm_ResizeInMemoryLicenseStore");
    }

    // Set Revocation Buffer
    err = Drm_Revocation_SetBuffer(appContext_.get(), pbRevocationBuffer_, REVOCATION_BUFFER_SIZE);
    if (DRM_FAILED(err)) {
        return NfError(err, DRMSYSTEM_INITIALIZATION_ERROR, "Error in Drm_Revocation_SetBuffer");
    }

#ifdef NRDP_HAS_TRACING
    // Trace out DRM store hash
    Vuc drmStoreHash;
    NFErr nfErr = getDrmStoreHash(drmStoreHash);
    if (nfErr == NFErr_OK) {
        MYTRACE("DRMStore hash %s", vectorToBase64String(drmStoreHash).c_str());
    } else {
        MYTRACE("Error: Unable to compute DRMStore hash");
    }
#endif

    return NFErr_OK;
}

NFErr PlayReady3DrmSystem::teardown()
{
    MYTRACE();

    ScopedMutex lock(drmAppContextMutex_);

    if(!appContext_.get() ){
        MYTRACE("Error: no app context yet");
        return NFErr_Uninitialized;
    }

    // Clean outdated licenses from the store
    MYTRACE("Drm_StoreMgmt_CleanupStore");
    DRM_RESULT err;
    err = Drm_StoreMgmt_CleanupStore(
            appContext_.get(),
            DRM_STORE_CLEANUP_ALL,
            NULL,
            0,
            NULL);
    if (DRM_FAILED(err)) {
        return NfError(err, DRMSYSTEM_DRMSTORE_ERROR, "Error in Drm_StoreMgmt_CleanupStore");
    }

    // Uninitialize drm context
    MYTRACE("Drm_Uninitialize");
    Drm_Uninitialize(appContext_.get());
    appContext_.reset();

    // Uninitialize platform
    MYTRACE("Drm_Platform_Uninitialize");
    err = Drm_Platform_Uninitialize(NULL);
    if (DRM_FAILED(err)) {
        return NfError(err, DRMSYSTEM_INITIALIZATION_ERROR, "Error in Drm_Platform_Uninitialize");
    }

    return NFErr_OK;
}

std::string PlayReady3DrmSystem::getDrmVersion() const
{
    const uint32_t MAXLEN = 64;
    DRM_CHAR versionStr[MAXLEN];
    if (g_dstrReqTagPlayReadyClientVersionData.cchString >= MAXLEN)
        return "";
    DRM_UTL_DemoteUNICODEtoASCII(g_dstrReqTagPlayReadyClientVersionData.pwszString,
            versionStr, MAXLEN);
    ((DRM_BYTE*)versionStr)[g_dstrReqTagPlayReadyClientVersionData.cchString] = 0;
    PackedCharsToNative(versionStr, g_dstrReqTagPlayReadyClientVersionData.cchString + 1);
    return std::string(versionStr);
}

NFErr PlayReady3DrmSystem::createDrmSession(const std::string& contentId,
                                           enum DrmLicenseType licenseType,
                                           const std::vector<uint8_t>& drmHeader,
                                           std::shared_ptr<IDrmSession>& drmSession)
{
    MYTRACE();

    if (sessionId_ >= UINT_MAX)
        sessionId_ = 0;

    // create a PlayReady3DrmSession
    drmSession.reset(new PlayReady3DrmSession(sessionId_++, contentId, licenseType,
            drmHeader, shared_from_this()));

    MYTRACE("%d, use_count %ld\n", sessionId_ - 1, drmSession.use_count());

    return NFErr_OK;
}

NFErr PlayReady3DrmSystem::destroyDrmSession(std::shared_ptr<IDrmSession> drmSession)
{
    MYTRACE("%d, use count %ld\n", drmSession->getSessionId(), drmSession.use_count());
    if (drmSession.get())
        drmSession->finalizeDrmSession();
    drmSession.reset();
    return NFErr_OK;
}

netflix::NFErr PlayReady3DrmSystem::getSecureStopIds(std::vector<std::vector<unsigned char> > &ids)
{
    ScopedMutex lock(drmAppContextMutex_);

    DRM_DWORD nSsSessionIds = 0;
    DRM_ID *ssSessionIds = NULL;
    MYTRACE("Drm_SecureStop_EnumerateSessions()");
    DRM_RESULT err;
    err = Drm_SecureStop_EnumerateSessions(
            appContext_.get(),
            playready3MeteringCertSize,
            playready3MeteringCert,
            &nSsSessionIds,
            &ssSessionIds);
    if (err != DRM_SUCCESS && err != DRM_E_NOMORE) {
        return NfError(err, DRMSYSTEM_SECURESTOP_ERROR, "Error in Drm_SecureStop_EnumerateSessions");
    }
    assert(nSsSessionIds == 0 || ssSessionIds != NULL);
    ids.clear();
    Vuc id(DRM_ID_SIZE, 0);
    for (uint32_t i = 0; i < nSsSessionIds; ++i)
    {
        assert(ssSessionIds[i].rgb);
        assert(sizeof(ssSessionIds[i].rgb) == DRM_ID_SIZE);
        std::copy(ssSessionIds[i].rgb, ssSessionIds[i].rgb + DRM_ID_SIZE, id.begin());
        ids.push_back(id);
    }
    Oem_MemFree(ssSessionIds);

    if (nSsSessionIds) {
        MYTRACE("found %d pending secure stop%s", nSsSessionIds, (nSsSessionIds > 1) ? "s" : "");
    }

    return NFErr_OK;
}

netflix::NFErr PlayReady3DrmSystem::getSecureStop(const std::vector<unsigned char> &ssSessionId,
                                                 std::shared_ptr<ISecureStop> &secureStop)
{
    ScopedMutex lock(drmAppContextMutex_);

    //MYTRACE("%s", vectorToBase64String(ssSessionId).c_str());

    std::vector<Vuc > ids;
    NFErr nfErr = getSecureStopIds(ids);
    if (nfErr == NFErr_OK && ids.size())
    {
        MYTRACE("%d secure stop%s pending commit", ids.size(), (ids.size() > 1) ? "s" : "");
        //for (std::vector<Vuc >::iterator it = ids.begin(); it != ids.end(); ++it)
        //    MYTRACE("\t%s", vectorToBase64String(*it).c_str());
    }

    // Get the secure stop challenge
    DRM_ID ssSessionDrmId;
    vectorIdToDrmId(ssSessionId, ssSessionDrmId);
    DRM_DWORD ssChallengeSize;
    DRM_BYTE *ssChallenge;
    MYTRACE("Drm_SecureStop_GenerateChallenge()");
    DRM_RESULT err = Drm_SecureStop_GenerateChallenge(
            appContext_.get(),
            &ssSessionDrmId,
            playready3MeteringCertSize,
            playready3MeteringCert,
            0, NULL, // no custom data
            &ssChallengeSize,
            &ssChallenge);
    if (err != DRM_SUCCESS) {
        return NfError(err, DRMSYSTEM_SECURESTOP_ERROR, "Error in Drm_SecureStop_GenerateChallenge");
    }
    MYTRACE("challengeSize=%d pChallenge=%p", ssChallengeSize, ssChallenge);

    //std::string ssStr((const char *)ssChallenge, ssChallengeSize);
    //MYTRACE("secure stop\n%s", ssStr.c_str());

    // create a new secureStop for return parm
    secureStop = std::shared_ptr<ISecureStop>(new DrmSecureStop(ssSessionId, ssChallenge, ssChallengeSize));
    Oem_MemFree(ssChallenge);  // free up PR3-allocated secure stop mem, now that we have our own copy

    return NFErr_OK;
}

void PlayReady3DrmSystem::commitSecureStop(const std::vector<unsigned char> &ssSessionId,
                                          const std::vector<unsigned char> &serverResponse)
{
    ScopedMutex lock(drmAppContextMutex_);

    MYTRACE();

    if (!ssSessionId.size()) {
        Log::error(TRACE_DRMSYSTEM, "Error: empty session id");
        return;
    }
    if (!serverResponse.size()) {
        return;
    }

    //std::string respStr(serverResponse.begin(), serverResponse.end());
    //MYTRACE("\n%s", respStr.c_str());

    DRM_ID sessionDrmId;
    vectorIdToDrmId(ssSessionId, sessionDrmId);
    DRM_DWORD customDataSizeBytes = 0;
    DRM_CHAR *pCustomData = NULL;
    MYTRACE("Drm_SecureStop_ProcessResponse()");
    DRM_RESULT err;
    err = Drm_SecureStop_ProcessResponse(
        appContext_.get(),
        &sessionDrmId,
        playready3MeteringCertSize,
        playready3MeteringCert,
        serverResponse.size(),
        &serverResponse[0],
        &customDataSizeBytes,
        &pCustomData);
    if (err == DRM_SUCCESS)
    {
        MYTRACE("secure stop commit successful");
        if (pCustomData && customDataSizeBytes)
        {
            // We currently don't use custom data from the server. Just log here.
            std::string customDataStr(pCustomData, customDataSizeBytes);
            MYTRACE("custom data = \"%s\"", customDataStr.c_str());
        }
    }
    else
    {
        Log::error(TRACE_DRMSYSTEM, "Drm_SecureStop_ProcessResponse returned 0x%lx", err);
    }
    Oem_MemFree(pCustomData);
}

int PlayReady3DrmSystem::resetSecureStops()
{
    // PlayReady3 does not provide any way to do this.
    return 0;
}

void PlayReady3DrmSystem::enableSecureStop(bool use)
{
    // Secure stop in PlayReady3 is a compile-time option only. We've configured
    // our build to enable it and it cannot be disabled at runtime.
    NRDP_UNUSED(use);
    MYTRACE("function not used for Playready3, noop");
}

bool PlayReady3DrmSystem::isSecureStopEnabled()
{
    // Secure stop in PlayReady3 is a compile-time option only. We've configured
    // our build to enable it so we must always return true here.
    MYTRACE("function not used for Playready3, returning true always");
    return true;
}

NFErr PlayReady3DrmSystem::deleteDrmStore()
{
    // As a linux reference implementation, we are cheating a bit by just using
    // stdio to delete the drm store from the filesystem. A real implementation
    // will be more complicated.
    MYTRACE();
    assert(drmStorePath_.size());
    if (remove(drmStorePath_.c_str()) != 0) {
        return NfError(errno, DRMSYSTEM_DRMSTORE_ERROR, "Error removing DRM store file");
    }
    return NFErr_OK;
}

NFErr PlayReady3DrmSystem::deleteKeyStore()
{
    // There is no keyfile in PlayReady3, so we cannot implement this function.
    MYTRACE("Function not implemented");
    return NFErr_NotImplemented;
}

NFErr PlayReady3DrmSystem::getDrmStoreHash(std::vector<unsigned char>& drmStoreHash)
{
    // As a linux reference implementation, we are cheating by just reading the
    // drmstore file from the filesystem and using openssl to compute a hash
    // over its bytes. A real implementation would be more complicated and
    // involve the TEE.
    bool success = calcFileSha256(drmStorePath_, drmStoreHash);
    if (!success) {
        return NfError(errno, DRMSYSTEM_DRMSTORE_ERROR, "Error computing DRM store file hash");
    }
    return NFErr_OK;
}

netflix::NFErr PlayReady3DrmSystem::getKeyStoreHash(std::vector<unsigned char>& /*keyStoreHash*/)
{
    // There is no keyfile in PlayReady3, so we cannot implement this function.
    MYTRACE("Function not implemented");
    return NFErr_NotImplemented;
}

// FIXME: This function reports the max number of ANY type of license that can
// be stored in the in-memory license store without triggering an expensive
// resize. So the name of this method is misleading.
netflix::NFErr PlayReady3DrmSystem::getLdlSessionsLimit(uint32_t& ldlLimit)
{
    MYTRACE();
    ldlLimit = NONCE_STORE_SIZE;
    return NFErr_OK;
}

time_t PlayReady3DrmSystem::getDrmTime() const
{
    // Playready3 supports client time completely within the opaque blobs sent
    // between the Playready client and server, so this function should really
    // not have to return a real time. However, the Netflix server still needs
    // a good client time for legacy reasons.
    // In this reference DPI we are cheating my just returning the linux system
    // time. A real implementation would be more complicated, perhaps getting
    // time from some sort of secure and/or anti-rollback resource.
    ScopedMutex lock(drmAppContextMutex_);
    return time(NULL);
}

std::shared_ptr<DRM_APP_CONTEXT> PlayReady3DrmSystem::getDrmAppContext()
{
    ScopedMutex lock(drmAppContextMutex_);
    return appContext_;
}

Mutex& PlayReady3DrmSystem::getDrmAppContextMutex()
{
    ScopedMutex lock(drmAppContextMutex_);
    return drmAppContextMutex_;
}

// --- Utility namespace-scoped free functions ---

NFErr makeNfError(DRM_RESULT result, esplayer::Status code, const char *func, const char *msg)
{
    const std::string resultStr = drmResultStr(result);
    NTRACE(TRACE_DRMSYSTEM, "%s: %s: 0x%08lx%s", func, msg,  static_cast<unsigned long>(result), resultStr.c_str());
    std::ostringstream ss;
    ss << func << ":" << msg << " " << resultStr;
    Variant errInfo;
    errInfo["errorDescription"] = ss.str();
    return NFErr(new IDeviceError(code, result, errInfo));
}

// Converts a PR3 DRM_ID to a std::vector. Assumes everything is sized correctly.
std::vector<unsigned char> drmIdToVectorId(const DRM_ID *drmId)
{
    assert(drmId);
    assert(sizeof(drmId->rgb) == DRM_ID_SIZE);
    if (!drmId)
        return std::vector<unsigned char>();
    return std::vector<unsigned char>(drmId->rgb, drmId->rgb + DRM_ID_SIZE);
}

// Converts a std::vector id to a PR3 DRM_ID. Assumes everything is sized correctly.
void vectorIdToDrmId(const std::vector<unsigned char>& src, DRM_ID& dst)
{
    assert(src.size() == DRM_ID_SIZE);
    assert(dst.rgb);
    assert(sizeof(dst.rgb) == DRM_ID_SIZE);
    if ( !dst.rgb || (sizeof(dst.rgb) != DRM_ID_SIZE) || (src.size() < DRM_ID_SIZE) )
        return;
    memcpy((void *)dst.rgb, (const void *)&src[0], DRM_ID_SIZE);
}

std::string vectorToBase64String(const std::vector<uint8_t>& vec)
{
    const std::vector<unsigned char> vecB64(Base64::encode(vec));
    return std::string(vecB64.begin(), vecB64.end());
}

// Returns a string of the form PlayReady*::<method>, trimming the return type,
// modifiers and arguments from what __PRETTY_FUNCTION__ gives you.
// This function is used by the NfError macro.
std::string methodName(const std::string& prettyFunction)
{
    size_t begin = prettyFunction.find("PlayReady");
    size_t end = prettyFunction.rfind("(") - begin;
    return prettyFunction.substr(begin,end);
}

#include "PlayReady3ErrorLookup.inc"

}} // namespace netflix::device
