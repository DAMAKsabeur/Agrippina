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

#include <nrdbase/Configuration.h>
#include <nrdbase/ReadDir.h>
#include <nrd/AppLog.h>
#include <nrd/IDeviceError.h>

#include "ESPlayerConstants.h"
#include "PlayReadyDrmSystem.h"
#include "PlayReadyDrmSession.h"

#include <drmversionconstants.h>

using namespace netflix;
using namespace netflix::device;
using namespace netflix::device::esplayer;

extern DRM_CONST_STRING g_dstrDrmPath;
DRM_WCHAR* PlayReadyDrmSystem::drmdir_;
DRM_CONST_STRING PlayReadyDrmSystem::drmStore_;
std::string PlayReadyDrmSystem::drmStoreStr_;
std::shared_ptr<DRM_APP_CONTEXT> PlayReadyDrmSystem::appContext_;
Mutex PlayReadyDrmSystem::drmAppContextMutex_(DRMAPPCONTEXT_MUTEX, "PlayReadyDrmSystem::drmAppContextMutex");

namespace {

// The rights we want to request.
// const DRM_WCHAR PLAY[] = { ONE_WCHAR('P', '\0'),
//                            ONE_WCHAR('l', '\0'),
//                            ONE_WCHAR('a', '\0'),
//                            ONE_WCHAR('y', '\0'),
//                            ONE_WCHAR('\0', '\0')
// };
// const DRM_CONST_STRING PLAY_RIGHT = CREATE_DRM_STRING(PLAY);
// const DRM_CONST_STRING* RIGHTS[] = { &PLAY_RIGHT };

/**
 * Creates a new DRM_WCHAR[] on the heap from the provided string.
 *
 * @param[in] s the string.
 * @return the resulting DRM_WCHAR[], NULL terminated.
 */
DRM_WCHAR* createDrmWchar(std::string const& s)
{
    DRM_WCHAR* w = new DRM_WCHAR[s.length() + 1];
    for (size_t i = 0; i < s.length(); ++i)
        w[i] = ONE_WCHAR(s[i], '\0');
    w[s.length()] = ONE_WCHAR('\0', '\0');
    return w;
}

// copied from Playready3 oem/linux/oemfileio.c
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

const std::string DrmSystem::type = "PlayReady";

PlayReadyDrmSystem::PlayReadyDrmSystem()
  :mSessionId(0)
{
    NTRACE(TRACE_DRMSYSTEM, "PlayReadyDrmSystem::%s", __func__);

    appContext_.reset();
    appContextOpaqueBuffer_ = NULL;
    pbRevocationBuffer_ = NULL;

    const std::string rdir = Configuration::dataReadPath() + "/dpi/playready";
    const std::string wdir = Configuration::dataWritePath() + "/dpi/playready/storage/";
    ReadDir::recursiveMakeDirectory(wdir);
    const std::string store = wdir + "drmstore";

    // Create wchar strings from the arguments.
    drmdir_ = createDrmWchar(rdir);

    // Initialize PlayReady directory.
    g_dstrDrmPath.pwszString = drmdir_;
    g_dstrDrmPath.cchString = rdir.length();

    // Save the DRM store location for later.
    drmStore_.pwszString = createDrmWchar(store);
    drmStore_.cchString = store.length();
    drmStoreStr_ = store;

    appContextOpaqueBuffer_ = new DRM_BYTE[MINIMUM_APPCONTEXT_OPAQUE_BUFFER_SIZE];
    pbRevocationBuffer_ = new DRM_BYTE[REVOCATION_BUFFER_SIZE];
}

PlayReadyDrmSystem::~PlayReadyDrmSystem()
{
    ScopedMutex lock(drmAppContextMutex_);
    if(appContext_.get() != NULL)
    {
        Drm_Reader_Commit(appContext_.get(), NULL, NULL);
        Drm_Uninitialize(appContext_.get());
        appContext_.reset();
    }

    delete [] drmdir_; drmdir_ = NULL;
    if (drmStore_.pwszString != NULL)
    {
        delete [] drmStore_.pwszString;
        drmStore_.pwszString = NULL;
        drmStore_.cchString = 0;
    }

    if (appContextOpaqueBuffer_ != NULL)
    {
        delete [] appContextOpaqueBuffer_;
        appContextOpaqueBuffer_ = NULL;
    }
    if (pbRevocationBuffer_ != NULL)
    {
        delete [] pbRevocationBuffer_;
        pbRevocationBuffer_ = NULL;
    }
}

NFErr PlayReadyDrmSystem:: initialize(const std::shared_ptr<IDrmSystemCallback>& drmSystemCallback)
{
    NRDP_UNUSED(drmSystemCallback);
    return initialize();
}

NFErr PlayReadyDrmSystem::initialize()
{
    NTRACE(TRACE_DRMSYSTEM, "PlayReadyDrmSystem::%s", __func__);
    Log::warn(TRACE_DRMSYSTEM, "%s %s", getDrmSystemType().c_str(), getDrmVersion().c_str());

    ScopedMutex lock(drmAppContextMutex_);

    DRM_RESULT err;

    // DRM Platform Initialization
    err = Drm_Platform_Initialize();
    if(DRM_FAILED(err))
    {
        NTRACE(TRACE_DRMSYSTEM, "PlayReadyDrmSystem::%s Error in Drm_Platform_Initialize  0x%08lx",
               __func__, static_cast<unsigned long>(err));
        appContext_.reset();
        std::ostringstream ss;
        ss << "Error in Drm_Platform_Initialize" << std::showbase << std::hex << err;
        Variant errInfo;
        errInfo["errorDescription"] = ss.str();
        return NFErr(new IDeviceError(DRMSYSTEM_INITIALIZATION_ERROR, err, errInfo));
    }

    appContext_.reset(new DRM_APP_CONTEXT);
    memset(appContext_.get(), 0, sizeof(DRM_APP_CONTEXT));
    err  = Drm_Initialize(appContext_.get(), NULL,
                          appContextOpaqueBuffer_,
                          MINIMUM_APPCONTEXT_OPAQUE_BUFFER_SIZE,
                          &drmStore_);
    if(DRM_FAILED(err)) {
        NTRACE(TRACE_DRMSYSTEM, "PlayReadyDrmSystem::initDrmContext: Error in Drm_Initialize:"
               " 0x%08lx", static_cast<unsigned long>(err));
        appContext_.reset();
        std::ostringstream ss;
        ss << "PlayReadyDrmSystem::initialize Error in Drm_Initialize:"<< std::showbase << std::hex << err;
        Variant errInfo;
        errInfo["errorDescription"] = ss.str();
        return NFErr(new IDeviceError(DRMSYSTEM_INITIALIZATION_ERROR, err, errInfo));
    }

    err = Drm_Revocation_SetBuffer( appContext_.get(), pbRevocationBuffer_, REVOCATION_BUFFER_SIZE);
    if(DRM_FAILED(err))
    {
        NTRACE(TRACE_DRMSYSTEM, "PlayReadyDrmSystem::initDrmContext: Error in Drm_Revocation_SetBuffer:"
                   " 0x%08lx", static_cast<unsigned long>(err));
        appContext_.reset();
        std::ostringstream ss;
        ss << "PlayReadyDrmSystem::initDrmContext: Error in Drm_Revocation_SetBuffer:"
           << std::showbase << std::hex << err;
        Variant errInfo;
        errInfo["errorDescription"] = ss.str();
        return NFErr(new IDeviceError(DRMSYSTEM_INITIALIZATION_ERROR, err, errInfo));
    }

   return NFErr_OK;
}

NFErr PlayReadyDrmSystem::teardown()
{
    NTRACE(TRACE_DRMSYSTEM, "PlayReadyDrmSystem::%s", __func__);

    ScopedMutex lock(drmAppContextMutex_);

    if( !appContext_.get() ){
        NTRACE(TRACE_DRMSYSTEM, "PlayReadyDrmSystem::%s - no app context yet", __func__);
        return NFErr_Uninitialized;
    }

    // cleanup drm store for expired or removal date license
    NFErr nferr = NFErr_OK;
    DRM_RESULT err;
    err = Drm_Reader_Commit(appContext_.get(), NULL, NULL);
    if(DRM_FAILED(err)){
        // nothing that we can do about. Just log
        NTRACE(TRACE_DRMSYSTEM, "PlayReadyDrmSystem::%s Drm_Reader_Commit 0x%08lx",
               __func__, static_cast<unsigned long>(err));
    }

    err = Drm_StoreMgmt_CleanupStore(appContext_.get(),
                                     DRM_STORE_CLEANUP_DELETE_EXPIRED_LICENSES |
                                     DRM_STORE_CLEANUP_DELETE_REMOVAL_DATE_LICENSES,
                                     NULL, 0, NULL);
    if(DRM_FAILED(err))
    {
        NTRACE(TRACE_DRMSYSTEM, "PlayReadyDrmSystem::%s Drm_StoreMgmt_CleanupStore 0x%08lx",
               __func__, static_cast<unsigned long>(err));
        appContext_.reset();
        std::ostringstream ss;
        ss << "Drm_StoreMgmt_CleanupStore" << std::showbase << std::hex << err;
        Variant errInfo;
        errInfo["errorDescription"] = ss.str();
        nferr.push(new IDeviceError(DRMSYSTEM_DRMSTORE_ERROR, err, errInfo));
    }
    // Uninitialize drm context
    Drm_Uninitialize(appContext_.get());
    appContext_.reset();

    // Unitialize platform
    err = Drm_Platform_Uninitialize();
    if(DRM_FAILED(err))
    {
        NTRACE(TRACE_DRMSYSTEM, "PlayReadyDrmSystem::%s Error in Drm_Platform_Uninitialize  0x%08lx",
               __func__, static_cast<unsigned long>(err));
        appContext_.reset();
        std::ostringstream ss;
        ss << "Error in Drm_Platform_Uninitialize" << std::showbase << std::hex << err;
        Variant errInfo;
        errInfo["errorDescription"] = ss.str();
        nferr.push(new IDeviceError(DRMSYSTEM_INITIALIZATION_ERROR, err, errInfo));
    }

    return nferr;
}

std::string PlayReadyDrmSystem::getDrmSystemType()
{
    return "PlayReady";
}

std::string PlayReadyDrmSystem::getDrmVersion() const
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

/**
 * IDrmSession factory methods
 */
NFErr PlayReadyDrmSystem::createDrmSession(std::shared_ptr<IDrmSession>& drmSession)
{
    NTRACE(TRACE_DRMSYSTEM, "PlayReadyDrmSystem::%s", __func__);

    if(mSessionId >= UINT_MAX)
        mSessionId = 0;

    // create a PlayReadyDrmSession
    drmSession.reset(new PlayReadyDrmSession(mSessionId++, shared_from_this()));

    NTRACE(TRACE_DRMSYSTEM, "[DrmSessionLifeCycle] %s for %d, use_count %ld\n",
           __func__, mSessionId - 1, drmSession.use_count());

    return NFErr_OK;
}

NFErr PlayReadyDrmSystem::createDrmSession(const std::string& contentId,
                                           enum DrmLicenseType licenseType,
                                           const std::vector<uint8_t>& drmHeader,
                                           std::shared_ptr<IDrmSession>& drmSession)
{
    NTRACE(TRACE_DRMSYSTEM, "PlayReadyDrmSystem::%s", __func__);

    if(mSessionId >= UINT_MAX)
        mSessionId = 0;

    // create a PlayReadyDrmSession
    drmSession.reset(new PlayReadyDrmSession(mSessionId++,
                                             contentId,
                                             licenseType,
                                             drmHeader,
                                             shared_from_this()));

    //NTRACE(TRACE_DRMSYSTEM, "[DrmSessionLifeCycle] %s for %d, use_count %ld\n",
    //       __func__, mSessionId - 1, drmSession.use_count());

    return NFErr_OK;
}

NFErr PlayReadyDrmSystem::destroyDrmSession(std::shared_ptr<IDrmSession> drmSession)
{
    //NTRACE(TRACE_DRMSYSTEM,
    //       "[DrmSessionLifeCycle] %s for %d, use count %ld\n",
    //       __func__, drmSession->getSessionId(), drmSession.use_count());

    if(drmSession.get())
    {
        drmSession->finalizeDrmSession();
    }
    drmSession.reset();
    return NFErr_OK;
}

//////////
//
// SecureStop
//
//////////
class DrmSecureStop : public ISecureStop
{
private:
    std::vector<unsigned char> m_sessionID;
    std::vector<unsigned char> m_rawData;

public:
    DrmSecureStop(const std::vector<unsigned char> &sessionID, size_t dataSize)
        : m_sessionID(sessionID)
    {
        m_rawData.resize(dataSize);
    }
    virtual ~DrmSecureStop() {}

    virtual unsigned char *getRawData()
    {
        return &m_rawData[0];
    }
    virtual size_t getRawDataSize()
    {
        return m_rawData.size();
    }
    virtual void getSessionID(std::vector<unsigned char> &sessionID)
    {
        sessionID = m_sessionID;
    }
};

/**
 *
 * SecureStop interfaces
 *
 */
/**
 * Returns all the secure stop ids used by the device
 *
 * @param[out] sessionIds vector of session ids (16 byte vector)
 * @return NFErr_OK if successful;
 *         NFErr_Bad if something went wrong;
 *         NFErr_NotAllowed if secure stop is not supported
 *         (e.g. Janus and PlayReady).
 */
netflix::NFErr PlayReadyDrmSystem::getSecureStopIds(std::vector<std::vector<unsigned char> > &ids)
{
    ScopedMutex lock(drmAppContextMutex_);

    // if secure stop is not supported, return NotAllowed
    DRM_BOOL supported = Drm_SupportSecureStop();
    if (supported == FALSE)
        return NFErr_NotAllowed;

    DRM_BYTE sessionIds[TEE_MAX_NUM_SECURE_STOPS][TEE_SESSION_ID_LEN];
    DRM_DWORD count = 0;
    DRM_RESULT err = Drm_GetSecureStopIds(appContext_.get(), sessionIds, &count);
    if (err != DRM_SUCCESS)
    {
        NTRACE(TRACE_DRMSYSTEM, "Drm_GetSecureStopIds returned 0x%lx", (long)err);
        std::ostringstream ss;
        ss << "PlayReadyDrmSystem::getSecureStopIds Error in Drm_GetSecureStopIds:"
           << std::showbase << std::hex << err;
        Variant errInfo;
        errInfo["errorDescription"] = ss.str();
        return NFErr(new IDeviceError(DRMSYSTEM_SECURESTOP_ERROR, err, errInfo));
    }

    // build up the returned ids
    ids.clear();
    for (unsigned int i=0; i<count; ++i) {
        std::vector<unsigned char> id(&sessionIds[i][0], &sessionIds[i][TEE_SESSION_ID_LEN]);
        ids.push_back(id);
    }

    return NFErr_OK;
}

/**
 * Provides the uncommitted secure stop tokens for all destroyed
 * DRM contexts.
 *
 * @param[in] sessionID sessionID of the secure stop to fetch
 * @param[out] current list of secure stops.
 * @return NFErr_OK if successful;
 *         NFErr_Bad if something went wrong;
 *         NFErr_NotAllowed if secure stop is not supported
 *         (e.g. Janus and PlayReady).
 */
netflix::NFErr PlayReadyDrmSystem::getSecureStop(const std::vector<unsigned char> &sessionID,
                                                 std::shared_ptr<ISecureStop> &secureStop /*out*/)
{
    ScopedMutex lock(drmAppContextMutex_);

    // if secure stop is not supported, return NotAllowed
    DRM_BOOL supported = Drm_SupportSecureStop();
    if (supported == FALSE)
        return NFErr_NotAllowed;

    if(!sessionID.size()){
        NTRACE(TRACE_DRMSYSTEM, "Drm_GetSecureStop sessionID length %zu", sessionID.size());
        std::ostringstream ss;
        ss << "Drm_GetSecureStop sessionID length:" << sessionID.size();
        Variant errInfo;
        errInfo["errorDescription"] = ss.str();
        return NFErr(new IDeviceError(DRMSYSTEM_SECURESTOP_ERROR,
                                      AgrippinaAppDeviceSpecific_InvalidDrmSecureStopId,
                                      errInfo));
    }

    // convert our vector to the uuid, sessionID is only supposed to be 16 bytes long
    unsigned char uuid[TEE_SESSION_ID_LEN];
    memcpy(&uuid[0], &sessionID[0], TEE_SESSION_ID_LEN);

    // call once with zero size to determine actual size of secure stop
    DRM_BYTE dummy;
    DRM_WORD rawSize = 0;
    DRM_RESULT err = Drm_GetSecureStop(appContext_.get(), uuid, &dummy, &rawSize);
    if (err != DRM_E_BUFFERTOOSMALL)
    {
        NTRACE(TRACE_DRMSYSTEM, "Drm_GetSecureStop(0) returned 0x%lx", (long)err);
        std::ostringstream ss;
        ss << "PlayReadyDrmSystem::getSecureStop Error in Drm_GetSecureStop(0):"
           << std::showbase << std::hex << err;
        Variant errInfo;
        errInfo["errorDescription"] = ss.str();
        return NFErr(new IDeviceError(DRMSYSTEM_SECURESTOP_ERROR, err, errInfo));
    }

    // create a new secureStop
    std::shared_ptr<DrmSecureStop> drmSecureStop(new DrmSecureStop(sessionID, rawSize));

    // now get the secure stop
    err = Drm_GetSecureStop(appContext_.get(), uuid, drmSecureStop->getRawData(),
                            &rawSize);
    if (err != DRM_SUCCESS)
    {
        NTRACE(TRACE_DRMSYSTEM, "Drm_GetSeureStop(1) returned 0x%lx", (long)err);
        std::ostringstream ss;
        ss << "PlayReadyDrmSystem::Drm_GetSecureStop(1) returned 0x%x" << std::showbase << std::hex << err;
        Variant errInfo;
        errInfo["errorDescription"] = ss.str();
        return NFErr(new IDeviceError(DRMSYSTEM_SECURESTOP_ERROR, err, errInfo));
    }

    // return the secure stop
    secureStop = drmSecureStop;
    return NFErr_OK;
}

/**
 * Commits a single secure stop token. The token will no longer be
 * available from getSecureStops().
 *
 * @param[in] sessionID sessionID of the secure stop to commit
 */
void PlayReadyDrmSystem::commitSecureStop(const std::vector<unsigned char> &sessionID,
                                          const std::vector<unsigned char> &serverResponse)
{
    NRDP_UNUSED(serverResponse);

    ScopedMutex lock(drmAppContextMutex_);

    // if secure stop is not supported, return
    DRM_BOOL supported = Drm_SupportSecureStop();
    if (supported == FALSE)
        return;

    NTRACE(TRACE_DRMSYSTEM, "PlayReadyDrmSystem::commitSecureStop");

    if(!sessionID.size()){
        NTRACE(TRACE_DRMSYSTEM, "Drm_GetSecureStop sessionID length %zu", sessionID.size());
        return;
    }

    // convert our vector to the uuid, sessionID is only supposed to be 16 bytes long
    unsigned char uuid[TEE_SESSION_ID_LEN];
    memcpy(&uuid[0], &sessionID[0], TEE_SESSION_ID_LEN);

    // commit it
    DRM_RESULT err = Drm_CommitSecureStop(appContext_.get(), uuid);
    if (err != DRM_SUCCESS)
    {
        Log::error(TRACE_DRMSYSTEM, "Drm_CommitSecureStop returned 0x%lx", (long)err);
    }
}

/**
 * Resets all persisted secure stops. This won't destroy any active playback session.
 *
 * @return number of secure stops deleted
 */
int PlayReadyDrmSystem::resetSecureStops()
{
    ScopedMutex lock(drmAppContextMutex_);

    // if secure stop is not supported, return
    DRM_BOOL supported = Drm_SupportSecureStop();
    if (supported == FALSE)
        return 0;

    DRM_WORD numDeleted = 0;
    DRM_RESULT err = Drm_ResetSecureStops(appContext_.get(), &numDeleted);
    if (err != DRM_SUCCESS)
    {
        Log::error(TRACE_DRMSYSTEM, "Drm_ResetSecureStops returned 0x%lx", (long)err);
    }
    return numDeleted;
}

/**
 * Enables/disables secure stop usage on this device
 */
void PlayReadyDrmSystem::enableSecureStop(bool use)
{
    ScopedMutex lock(drmAppContextMutex_);

    Drm_TurnSecureStop(static_cast<int>(use));
}

/**
 * Queries secure stop support on this device
 */
bool PlayReadyDrmSystem::isSecureStopEnabled()
{
    ScopedMutex lock(drmAppContextMutex_);

    return static_cast<bool>(Drm_SupportSecureStop());
}

/**
 *
 * Drm Key/License storage handling interfaces
 *
 */

/**
 * delete drm store
 * @return NFErr_OK if successful or if drm store is already empty
 *         NFErr_IOError if delete operation failed
 *         NFErr_NotImplemented if not implemented
 */
NFErr PlayReadyDrmSystem::deleteDrmStore()
{
    NTRACE(TRACE_DRMSYSTEM, "PlayReadyDrmSystem::deleteDrmStore");

    ScopedMutex lock(drmAppContextMutex_);

    NFErr nferr = NFErr_OK;

    DRM_RESULT err = Drm_DeleteSecureStore(&drmStore_);
    if (err != DRM_SUCCESS){
        Log::error(TRACE_DRMSYSTEM, "Drm_DeleteSecureStore returned 0x%lx", (long)err);
        std::ostringstream ss;
        ss << "PlayReadyDrmSystem::deleteDrmStore: Error in Drm_DeleteSecureStore:"
           << std::showbase << std::hex << err;
        Variant errInfo;
        errInfo["errorDescription"] = ss.str();
        nferr.push(new IDeviceError(DRMSYSTEM_DRMSTORE_ERROR, err, errInfo));
    }

    return nferr;
}

/**
 * delete key store
 * @return NFErr_OK if successful or if drm store is already empty
 *         NFErr_IOError if delete operation failed
 *         NFErr_NotImplemented if not implemented
 */
NFErr PlayReadyDrmSystem::deleteKeyStore()
{
    NTRACE(TRACE_DRMSYSTEM, "PlayReadyDrmSystem::deleteKeyStore");

    ScopedMutex lock(drmAppContextMutex_);

    NFErr nferr  = NFErr_OK;

    DRM_RESULT err = Drm_DeleteKeyStore();
    if (err != DRM_SUCCESS){
        Log::error(TRACE_DRMSYSTEM, "Drm_DeleteKeyStore returned 0x%lx", (long)err);
        std::ostringstream ss;
        ss << "PlayReadyDrmSystem::deleteKeyStore: Error in Drm_DeleteKeyStore:"
           << std::showbase << std::hex << err;
        Variant errInfo;
        errInfo["errorDescription"] = ss.str();
        nferr.push(new IDeviceError(DRMSYSTEM_KEYSTORE_ERROR, err, errInfo));
    }

    return nferr;
}

/**
 * get SHA256 hash of drm store
 * @param[out] drmStoreHash vector of hash SHA256 result
 * @return NFErr_OK if successful
 *         NFErr_IOError if it can not read drm store
 *         NFErr_NotImplemented if not implemented
 */
NFErr PlayReadyDrmSystem::getDrmStoreHash(std::vector<unsigned char> &drmStoreHash)
{
    ScopedMutex lock(drmAppContextMutex_);

    NTRACE(TRACE_DRMSYSTEM, "PlayReadyDrmSystem::getDrmStoreHash");

    NFErr nferr = NFErr_OK;

    drmStoreHash.resize(256);
    DRM_RESULT err = Drm_GetSecureStoreHash(&drmStore_, static_cast< unsigned char * > (&drmStoreHash[0]));
    if (err != DRM_SUCCESS){
        Log::error(TRACE_DRMSYSTEM, "Drm_GetSecureStoreHash returned 0x%lx", (long)err);
        std::ostringstream ss;
        ss << "PlayReadyDrmSystem::getKeyStoreHash: Error in Drm_GetKeyStoreHash:"
           << std::showbase << std::hex << err;
        Variant errInfo;
        errInfo["errorDescription"] = ss.str();
        nferr.push(new IDeviceError(DRMSYSTEM_DRMSTORE_ERROR, err, errInfo));
    }

    /*
    std::string hash(drmStoreHash.begin(), drmStoreHash.end());
    NTRACE(TRACE_DRMSYSTEM, "SHA256 hash for key: %s", hash.c_str());
    */

    return nferr;
}

/**
 * get SHA256 hash of key store
 * @param[out] drmStoreHash vector of hash SHA256 result
 * @return NFErr_OK if successful
 *         NFErr_IOError if it can not read drm store
 *         NFErr_NotImplemented if not implemented
 */
netflix::NFErr PlayReadyDrmSystem::getKeyStoreHash(std::vector<unsigned char> &keyStoreHash)
{
    ScopedMutex lock(drmAppContextMutex_);

    NTRACE(TRACE_DRMSYSTEM, "PlayReadyDrmSystem::getKeyStoreHash");

    NFErr nferr = NFErr_OK;

    keyStoreHash.resize(256);

    DRM_RESULT err = Drm_GetKeyStoreHash(&keyStoreHash[0]);
    if (err != DRM_SUCCESS){
        Log::error(TRACE_DRMSYSTEM, "Drm_DeleteKeyStore returned 0x%lx", (long)err);
        std::ostringstream ss;
        ss << "PlayReadyDrmSystem::getKeyStoreHash: Error in Drm_GetKeyStoreHash:"
           << std::showbase << std::hex << err;
        Variant errInfo;
        errInfo["errorDescription"] = ss.str();
        nferr.push(new IDeviceError(DRMSYSTEM_KEYSTORE_ERROR, err, errInfo));
    }

    /*
    std::string hash(keyStoreHash.begin(), keyStoreHash.end());
    NTRACE(TRACE_DRMSYSTEM, "SHA256 hash for key: %s", hash.c_str());
    */
    return nferr;
}

netflix::NFErr PlayReadyDrmSystem::getLdlSessionsLimit(uint32_t& ldlLimit)
{
    ScopedMutex lock(drmAppContextMutex_);

    NFErr nferr;
    DRM_RESULT err = Drm_LicenseAcq_GetLdlSessionsLimit_Netflix(appContext_.get(),
                                                                &ldlLimit);
    if (err != DRM_SUCCESS){
        Log::error(TRACE_DRMSYSTEM, "Drm_LicenseAcq_GetLdlSessionsLimit_Netflix 0x%lx", (long)err);
        std::ostringstream ss;
        ss << "PlayReadyDrmSystem::getLdlSessionsLimit :"
           << "Error in Drm_LicenseAcq_GetLdlSessionsLimit_Netflix:"
           << std::showbase << std::hex << err;
        Variant errInfo;
        errInfo["errorDescription"] = ss.str();
        nferr.push(new IDeviceError(DRMSYSTEM_LICENSE_ERROR, err, errInfo));
    }
    return nferr;
}

time_t PlayReadyDrmSystem::getDrmTime() const
{
    ScopedMutex lock(drmAppContextMutex_);

    DRM_UINT64 utctime64;
    DRM_RESULT err = Drm_Clock_GetSystemTime(appContext_.get(), &utctime64);
    if (err != DRM_SUCCESS){
        Log::error(TRACE_DRMSYSTEM, "Drm_Clock_GetSystemTime error 0x%lx", (long)err);
        // return invalid time
        return (time_t) -1;
    } else {
        NTRACE(TRACE_DRMSYSTEM, "DPI: getDrmTime %ld, time %ld\n", (time_t)utctime64, time(NULL));
        return (time_t)utctime64;
    }
}

/**
 * helper functions
 */

std::shared_ptr<DRM_APP_CONTEXT> PlayReadyDrmSystem::getDrmAppContext()
{
    return appContext_;
}

Mutex& PlayReadyDrmSystem::getDrmAppContextMutex()
{
    return drmAppContextMutex_;
}
