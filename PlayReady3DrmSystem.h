/*
 * (c) 1997-2016 Netflix, Inc.  All content herein is protected by
 * U.S. copyright and other applicable intellectual property laws and
 * may not be copied without the express permission of Netflix, Inc.,
 * which reserves all rights.  Reuse of any of this content for any
 * purpose without the permission of Netflix, Inc. is strictly
 * prohibited.
 */

#ifndef PLAYREADY3DRMSYSTEM_H
#define PLAYREADY3DRMSYSTEM_H

#ifndef PLAYREADY3
#error "You must define PLAYREADY3 to use this file"
#endif

#include <drmmanager.h>  // playready top-level API
/* Unset conflicting SAL tags. */
#undef __in
#undef __out

#include <nrd/IDrmSystem.h>

namespace netflix {
namespace device {

class PlayReady3DrmSystem : public IDrmSystem, public std::enable_shared_from_this<PlayReady3DrmSystem>
{
public:
    /*
     * For the devices that require tight coupling between player and drm session,
     * additional overloaded dpi defined in IDrmSystem need to be implemented
     */
    using IDrmSystem::initialize;
    using IDrmSystem::createDrmSession;

    PlayReady3DrmSystem();

    ~PlayReady3DrmSystem();

    virtual NFErr initialize();
    NFErr reinitialize();

    virtual NFErr teardown();

    virtual std::string getDrmVersion() const;

    /**
     *
     * IDrmSession factory methods
     *
     */
    virtual NFErr createDrmSession(const std::string& contentId,
                                   enum DrmLicenseType licenseType,
                                   const std::vector<uint8_t>& drmHeader,
                                   std::shared_ptr<IDrmSession>& drmSession);

    virtual NFErr destroyDrmSession(std::shared_ptr<IDrmSession> drmSession);

    /**
     *
     * SecureStop interfaces
     *
     */

    /**
     * Returns all the secure stop ids currently pending
     *
     * @param[out] secureStopIds vector of session ids (each is a 16 byte vector)
     * @return NFErr_OK if successful;
     *         NFErr_Bad if something went wrong;
     *         NFErr_NotAllowed if secure stop is not supported
     */
    virtual netflix::NFErr getSecureStopIds(
        /*out*/std::vector<std::vector<unsigned char> > &sessionIds);

    /**
     * Returns the secure stop for the requested secure stop ID, if one exists.
     * If the secure stop ID references a currently active playback session,
     * calling this function will cleanly shutdown playback and return the
     * secure stop.
     *
     * @param[in] secureStopId secureStopId of the secure stop to fetch
     * @param[out] secureStop secure stop token.
     * @return NFErr_OK if successful;
     *         NFErr_Bad if something went wrong;
     *         NFErr_NotAllowed if secure stop is not supported
     */
    virtual netflix::NFErr getSecureStop(const std::vector<unsigned char> &sessionID,
                                         /*out*/std::shared_ptr<ISecureStop> &secureStop);

    /**
     * Commits a single secure stop token, passing in the any response data
     * received from the server. The token will no longer be available from
     * getSecureStops().
     *
     * @param[in] secureStopId secureStopId of the secure stop to commit
     * @param[in] serverResponse response data from the server
     */
    virtual void commitSecureStop(const std::vector<unsigned char> &sessionID,
                                  const std::vector<unsigned char> &serverResponse);

    /**
     * Resets all persisted secure stops. This won't destroy any active playback session.
     *
     * @return number of secure stops deleted
     */
    virtual int resetSecureStops();

    /**
     * Enables/disables secure stop usage on this device. Some platforms may
     * ignore this function, depending on the capabilities of the DRM system in
     * use.
     */
    virtual void enableSecureStop(bool);

    /**
     * Queries secure stop support on this device
     */
    virtual bool isSecureStopEnabled();

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
    virtual netflix::NFErr deleteDrmStore();

     /**
     * delete key store
     * @return NFErr_OK if successful or if drm store is already empty
     *         NFErr_IOError if delete operation failed
     *         NFErr_NotImplemented if not implemented
     */
    virtual netflix::NFErr deleteKeyStore();

    /**
     * get SHA256 hash of drm store
     * @param[out] drmStoreHash vector of hash SHA256 result
     * @return NFErr_OK if successful
     *         NFErr_IOError if it can not read drm store
     *         NFErr_NotImplemented if not implemented
     */
    virtual netflix::NFErr getDrmStoreHash(std::vector<unsigned char> &drmStoreHash);

    /**
     * get SHA256 hash of key store
     * @param[out] drmStoreHash vector of hash SHA256 result
     * @return NFErr_OK if successful
     *         NFErr_IOError if it can not read drm store
     *         NFErr_NotImplemented if not implemented
     */
    virtual netflix::NFErr getKeyStoreHash(std::vector<unsigned char> &keyStoreHash);

    /**
     * get maximum LDL session counts supported by underlying drm system
     * @param[out] max number of ldl session that can be created
     * @return NFErr_Ok if successful
     *         NFErr_NotImplemented if not implemented
     */
    virtual netflix::NFErr getLdlSessionsLimit(uint32_t& ldlLimit);

    /**
     * Return the current time from the DRM subsystem.
     *
     * @return the DRM subsystem time.
     */
    // FIXME: Why should this be const?
    virtual time_t getDrmTime() const;

    /**
     * helper functions
     */
    std::shared_ptr<DRM_APP_CONTEXT> getDrmAppContext();
    Mutex& getDrmAppContextMutex();
    void registerSession(std::shared_ptr<IDrmSession> drmSession,
            const std::vector<unsigned char> &ssSessionId);

private:
    uint32_t sessionId_;
    DRM_WCHAR* drmdir_;         //!< Device certificate / key directory.
    DRM_CONST_STRING drmStore_; //!< DRM store filename.
    std::shared_ptr<DRM_APP_CONTEXT> appContext_;
    mutable Mutex drmAppContextMutex_;  // mutable required to support const fn getDrmTime()
    DRM_BYTE * const appContextOpaqueBuffer_;
    DRM_BYTE * const pbRevocationBuffer_;
    std::string drmStorePath_;
};

// Namespace-scoped utility free functions

std::vector<unsigned char> drmIdToVectorId(const DRM_ID *drmId);
void vectorIdToDrmId(const std::vector<unsigned char>& src, DRM_ID& dst);
std::string vectorToBase64String(const std::vector<uint8_t>& vec);

std::string drmResultStr(DRM_RESULT result);

NFErr makeNfError(DRM_RESULT result, esplayer::Status code, const char *func, const char *msg);
std::string methodName(const std::string& prettyFunction);
#define NfError(err1, err2, msg) (makeNfError(err1, err2, methodName(__PRETTY_FUNCTION__).c_str(), msg))

}}  // namespace netflix::device

#endif
