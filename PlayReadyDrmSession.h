/*
 * (c) 1997-2016 Netflix, Inc.  All content herein is protected by
 * U.S. copyright and other applicable intellectual property laws and
 * may not be copied without the express permission of Netflix, Inc.,
 * which reserves all rights.  Reuse of any of this content for any
 * purpose without the permission of Netflix, Inc. is strictly
 * prohibited.
 */

#ifndef PLAYREADYDRMSESSION_H
#define PLAYREADYDRMSESSION_H

#include <vector>

#include <nrdbase/tr1.h>
#include <nrdbase/StdInt.h>
#include <nrd/IDrmSession.h>

namespace netflix {
namespace device {

class LicenseResponse;

/**
 *
 *
 */

class PlayReadyDrmSession : public IDrmSession
{
public:
    PlayReadyDrmSession(uint32_t sessionId,
                        std::shared_ptr<PlayReadyDrmSystem> drmSystem);
    
    PlayReadyDrmSession(uint32_t sessionId,
                        std::string contentId,
                        enum DrmLicenseType licenseType,
                        const std::vector<uint8_t>& drmHeader,
                        std::shared_ptr<PlayReadyDrmSystem> drmSystem);

    ~PlayReadyDrmSession();

    virtual void finalizeDrmSession();

    virtual std::string getDrmType();

    virtual uint32_t getSessionId();

    virtual std::string getContentId();

    virtual enum DrmLicenseType getLicenseType();

    virtual void setSessionState(enum SessionState);

    virtual enum SessionState getSessionState();

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
    virtual NFErr getChallengeData(std::vector<uint8_t>& challenge, bool isLDL);

    virtual NFErr getChallengeData(std::vector<uint8_t>& challenge,
                                   const std::string& contentId,
                                   enum DrmLicenseType licenseType,
                                   const std::vector<uint8_t>& drmHeader);
    
    /**
     * Stores license and returns a session ID.  The session ID will be used to
     * identify secure stops.
     *
     * @param[in] licenseData: the license data
     * @param[in] licenseDataSize: license data size.
     * @param[out] secureStopId: specifies assigned secureStopId for current DRM session.
     *
     * @return OK on success, FURTHER_STAGES_REQUIRED if this is a
     *         multi-stage challenge and additional stages are needed, or
     *         ERROR.
     */
    virtual NFErr storeLicenseData(const std::vector<uint8_t>& licenseData,
                                   std::vector<unsigned char> &secureStopId);

    /**
     * Clear a previously stored DRM license.
     *
     * @return OK if succeeded, otherwise ERROR.
     */
    virtual NFErr clearLicense();


    virtual NFErr  decrypt(unsigned char* IVData,
                           uint32_t IVDataSize,
                           ullong dataSegmentOffset,
                           DataSegment& dataSegment);

    virtual NFErr initDecryptContext();

    virtual NFErr initDecryptContext(const std::string& /*keyId*/){ return initDecryptContext(); }
    virtual NFErr initDecryptContextByKid(const std::string& /*keyId*/) { return initDecryptContext(); }
        
    virtual void cleanupDecryptContext();

    void cancelChallenge();

    /**
     * Returns the compressed digital video minimum output protection
     * level. Zero until decrypt(std::vector<char>&) is called.
     *
     * @return compressed digital video protection level.
     */
    uint16_t compressedDigitalVideoLevel() const {
        return levels_.compressedDigitalVideoLevel_;
    }

    /**
     * Returns the uncompressed digital video minimum output
     * protection level. Zero until decrypt(std::vector<char>&) is
     * called.
     *
     * @return uncompressed digital video protection level.
     */
    uint16_t uncompressedDigitalVideoLevel() const {
        return levels_.uncompressedDigitalVideoLevel_;
    }

    /**
     * Returns the analog video minimum output protection level. Zero
     * until decrypt(std::vector<char>&) is called.
     *
     * @return analog video protection level.
     */
    uint16_t analogVideoLevel() const {
        return levels_.analogVideoLevel_;
    }

    /**
     * Returns the compressed digital audio minimum output protection
     * level. Zero until decrypt(std::vector<char>&) is called.
     *
     * @return compressed digital audio protection level.
     */
    uint16_t compressedDigitalAudioLevel() const {
        return levels_.compressedDigitalAudioLevel_;
    }

    /**
     * Returns the uncompressed digital audio minimum output
     * protection level. Zero until decrypt(std::vector<char>&) is
     * called.
     *
     * @return uncompressed digital audio protection level.
     */
    uint16_t uncompressedDigitalAudioLevel() const {
        return levels_.uncompressedDigitalAudioLevel_;
    }
    /** Playback protection levels struct. */
    struct PlayLevels {
        uint16_t compressedDigitalVideoLevel_;   //!< Compressed digital video output protection level.
        uint16_t uncompressedDigitalVideoLevel_; //!< Uncompressed digital video output protection level.
        uint16_t analogVideoLevel_;              //!< Analog video output protection level.
        uint16_t compressedDigitalAudioLevel_;   //!< Compressed digital audio output protection level.
        uint16_t uncompressedDigitalAudioLevel_; //!< Uncompressed digital audio output protection level.
    };

    Mutex& getDecryptContextMutex();

private:
    NFErr reinitializeForCurrentSession();

private:
    std::string mDrmType;
    enum IDrmSession::SessionState mSessionState;
    uint32_t mSessionId;
    std::string mContentId;
    enum DrmLicenseType mLicenseType;
    std::vector<uint8_t> mDrmHeader; // drm header is used not only for
                                     // generating a challenge, but also
                                     // deleting a license. So, it makes sense
                                     // to have it as a member

    std::vector<uint8_t> mNounce; // this is Netflix Specific PlayReady only
    std::vector<uint8_t> mSecureStopId;

    std::shared_ptr<PlayReadyDrmSystem> mDrmSystem;
    std::shared_ptr<DRM_APP_CONTEXT> mAppContext;
    std::shared_ptr<DRM_DECRYPT_CONTEXT> decryptContext_;
    Mutex mDrmDecryptContextMutex;
    struct PlayLevels levels_;
    std::auto_ptr<LicenseResponse> mLicenseResponse;
};

} // device
} // netflix

#endif
