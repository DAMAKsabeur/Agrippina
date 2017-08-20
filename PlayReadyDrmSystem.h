/*
 * (c) 1997-2016 Netflix, Inc.  All content herein is protected by
 * U.S. copyright and other applicable intellectual property laws and
 * may not be copied without the express permission of Netflix, Inc.,
 * which reserves all rights.  Reuse of any of this content for any
 * purpose without the permission of Netflix, Inc. is strictly
 * prohibited.
 */

#ifndef PLAYREADYDRMSYSTEM_H
#define PLAYREADYDRMSYSTEM_H

#include <vector>

// playready headers
#include <drmmanager.h>
#include <drmstrings.h>
/* Unset conflicting SAL tags. */
#undef __in
#undef __out

#include <nrdbase/tr1.h>
#include <nrdbase/StdInt.h>
#include <nrdbase/NFErr.h>
#include <nrd/IDrmSystem.h>
#include <nrd/IDrmSession.h>

namespace netflix {
namespace device {

class PlayReadyDrmSystem : public IDrmSystem, public std::enable_shared_from_this<PlayReadyDrmSystem>
{
public:
    PlayReadyDrmSystem();

    ~PlayReadyDrmSystem();
    
    virtual NFErr initialize();

    virtual NFErr initialize(const std::shared_ptr<IDrmSystemCallback>& drmSystemCallback);
    
    virtual NFErr teardown();

    virtual std::string getDrmSystemType();

    virtual std::string getDrmVersion() const;

    /**
     *
     * IDrmSession factory methods
     *
     */
    virtual NFErr createDrmSession(std::shared_ptr<IDrmSession>& drmSession);
    
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
     * Returns all the secure stop ids used by the device
     *
     * @param[out] sessionIds vector of session ids (16 byte vector)
     * @return NFErr_OK if successful;
     *         NFErr_Bad if something went wrong;
     *         NFErr_NotAllowed if secure stop is not supported
     *         (e.g. Janus and PlayReady).
     */
    virtual netflix::NFErr getSecureStopIds(
        /*out*/std::vector<std::vector<unsigned char> > &sessionIds);

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
    virtual netflix::NFErr getSecureStop(const std::vector<unsigned char> &sessionID,
                                         /*out*/std::shared_ptr<ISecureStop> &secureStop);

    /**
     * Commits a single secure stop token. The token will no longer be
     * available from getSecureStops().
     *
     * @param[in] sessionID sessionID of the secure stop to commit
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
     * Enables/disables secure stop usage on this device
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
    virtual time_t getDrmTime() const;


    /**
     * helper functions
     */
    static std::shared_ptr<DRM_APP_CONTEXT> getDrmAppContext();
    static Mutex& getDrmAppContextMutex();


private:
    uint32_t mSessionId;
    static DRM_WCHAR* drmdir_;         //!< Device certificate / key directory.
    static DRM_CONST_STRING drmStore_; //!< DRM store filename.
    static std::string drmStoreStr_;   //!< DRM store filename as string.
    static std::shared_ptr<DRM_APP_CONTEXT> appContext_;
    static Mutex drmAppContextMutex_;

#if defined (PLAYREADY2)
    DRM_BYTE *appContextOpaqueBuffer_;
    DRM_BYTE *pbRevocationBuffer_;
#endif

};

}}

#endif
