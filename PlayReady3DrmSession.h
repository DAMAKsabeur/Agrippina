/*
 * (c) 1997-2016 Netflix, Inc.  All content herein is protected by
 * U.S. copyright and other applicable intellectual property laws and
 * may not be copied without the express permission of Netflix, Inc.,
 * which reserves all rights.  Reuse of any of this content for any
 * purpose without the permission of Netflix, Inc. is strictly
 * prohibited.
 */

#ifndef PLAYREADY3DRMSESSION_H
#define PLAYREADY3DRMSESSION_H

#ifndef PLAYREADY3
#error "You must define PLAYREADY3 to use this file"
#endif

#include <nrd/IDrmSession.h>
#include <drmmanagertypes.h>

namespace netflix {
namespace device {

class PlayReady3DrmSystem;

typedef std::vector<uint8_t> Vui8;

struct OutputProtection {
    uint16_t compressedDigitalVideoLevel;   //!< Compressed digital video output protection level.
    uint16_t uncompressedDigitalVideoLevel; //!< Uncompressed digital video output protection level.
    uint16_t analogVideoLevel;              //!< Analog video output protection level.
    uint16_t compressedDigitalAudioLevel;   //!< Compressed digital audio output protection level.
    uint16_t uncompressedDigitalAudioLevel; //!< Uncompressed digital audio output protection level.
    uint32_t maxResDecodeWidth;             //!< Max res decode width in pixels.
    uint32_t maxResDecodeHeight;            //!< Max res decode height in pixels.
    OutputProtection();
    void setOutputLevels(const DRM_MINIMUM_OUTPUT_PROTECTION_LEVELS& mopLevels);
    void setMaxResDecode(uint32_t width, uint32_t height);
};

class PlayReady3DrmSession : public IDrmSession, public std::enable_shared_from_this<PlayReady3DrmSession>
{
public:
    /*
     * For the devices that require tight coupling between player and drm session,
     * additional overloaded dpi defined in IDrmSession  need to be implemented
     */
    using IDrmSession::getChallengeData;
    
    PlayReady3DrmSession(uint32_t sessionId,
                        const std::string& contentId,
                        enum DrmLicenseType licenseType,
                        const std::vector<uint8_t>& drmHeader,
                        std::shared_ptr<PlayReady3DrmSystem> drmSystem);

    ~PlayReady3DrmSession();

    virtual void finalizeDrmSession();

    virtual std::string getDrmType();

    virtual uint32_t getSessionId() {return mSessionId;}

    virtual std::string getContentId() {return mContentId;}

    virtual enum DrmLicenseType getLicenseType() {return mLicenseType;}

    virtual void setSessionState(enum SessionState);

    virtual enum SessionState getSessionState() {return mSessionState;}

    virtual NFErr getChallengeData(std::vector<uint8_t>& challenge, bool isLDL);

    virtual NFErr storeLicenseData(const std::vector<uint8_t>& licenseData,
                                   std::vector<unsigned char> &secureStopId);

    virtual NFErr clearLicense();

    virtual NFErr  decrypt(unsigned char* IVData,
                           uint32_t IVDataSize,
                           ullong dataSegmentOffset,
                           DataSegment& dataSegment);

    virtual NFErr initDecryptContext();
    virtual NFErr initDecryptContextByKid(const std::string& keyId);

    virtual void cleanupDecryptContext();

    virtual bool hasLicense(const std::string& keyId);

    // Maxres decode
    virtual uint32_t maxResDecodeWidth() const;
    virtual uint32_t maxResDecodeHeight() const;

    // PlayReady-specific output protection levels
    uint16_t compressedDigitalVideoLevel() const;
    uint16_t uncompressedDigitalVideoLevel() const;
    uint16_t analogVideoLevel() const;
    uint16_t compressedDigitalAudioLevel() const;
    uint16_t uncompressedDigitalAudioLevel() const;

    Mutex& getDecryptContextMutex();

private:
    NFErr setDrmHeader();
    NFErr setKeyId(const Vui8& keyId);
    NFErr initDecryptContextPrivate(const Vui8& keyId);
    bool hasLicensePrivate(const Vui8& keyId);

private:
    std::string mDrmType;
    enum IDrmSession::SessionState mSessionState;
    uint32_t mSessionId;
    std::string mContentId;
    enum DrmLicenseType mLicenseType;
    Vui8 mDrmHeader;
    std::shared_ptr<PlayReady3DrmSystem> mDrmSystem;
    std::shared_ptr<DRM_APP_CONTEXT> mAppContext;
    struct DecryptContext
    {
        DRM_DECRYPT_CONTEXT drmDecryptContext;
        OutputProtection outputProtection;
        DecryptContext();
    };
    typedef std::map<Vui8, std::shared_ptr<DecryptContext> > DecryptContextMap;
    DecryptContextMap mDecryptContextMap;
    Mutex mDrmDecryptContextMutex;
    Vui8 mBatchId;
    typedef std::vector<Vui8> Vu8List;
    Vu8List mLicenseIds;
    Vu8List mKeyIds;
    std::shared_ptr<DecryptContext> mActiveDecryptContext;
};

}} // namespace netflix::device

#endif
