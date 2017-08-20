/*
 * (c) 1997-2016 Netflix, Inc.  All content herein is protected by
 * U.S. copyright and other applicable intellectual property laws and
 * may not be copied without the express permission of Netflix, Inc.,
 * which reserves all rights.  Reuse of any of this content for any
 * purpose without the permission of Netflix, Inc. is strictly
 * prohibited.
 */
#ifndef FILESYSTEM_H
#define FILESYSTEM_H

/** @file FileSystem.h - Reference implementation of the ISystem interface.
 *
 * A device partner may use or modify this header and accompanying .cpp file as
 * needed. For example, the device partner should modify the capabilities
 * reported by ISystem::getCapability() to be consistent with their particular
 * device's capabilities.
 */

#include <vector>

#include <nrd/ISystem.h>

#include <nrdbase/Application.h>
#include <nrdbase/ConditionVariable.h>
#include <nrdbase/ScopedMutex.h>
#include "NetworkInterfaceListener.h"
#include "AgrippinaESManager.h"
#include "SystemAudioManager.h"

namespace netflix {

class TeeApiStorage;

namespace device {

/**
 * @class FileSystem FileSystem.h
 * @brief A file-based implementation to system information used by
 *        the Netflix application.
 */
class FileSystem : public ISystem
{
public:
    /**
     * Create a new file-based system implementation that doesn't have
     * an ESN, keys, or other device identification data.
     *
     * This is used for testing.
     *
     * @param[in] encryptedFile encrypted storage file location.
     */
    FileSystem(const std::string& encryptedFile,
               const std::string& pubkeyFile = std::string(),
               const std::string& esn = std::string(),
               const std::string& model = std::string());

    /** Destructor */
    virtual ~FileSystem();

    /** @sa ISystem::init(EventDispatcher&) */
    virtual void init(std::shared_ptr<EventDispatcher> eventDispatcher);

    /** @sa ISystem::shutdown() */
    virtual void shutdown();

    /** @sa ISystem::shutdown() */
    virtual void requestSuspend(const std::string &reason);

    /** @sa ISystem::isScreensaverOn() */
    virtual bool isScreensaverOn() const;

    /** @sa ISystem::getReport() */
    virtual Variant getReport();

    /** @sa ISystem::getUptime() */
    virtual void getUptime(uint32_t & uptime, uint32_t & timeSinceUserOn);

    /** @sa ISystem::setRegistered(bool) */
    virtual void setRegistered(bool registered);

    /** @sa ISystem::storeEncrypted(const std::map<std::string,std::string>&) */
    virtual void storeEncrypted(const std::map<std::string,std::string>& data);

    virtual void flushEncrypted();

    /** @sa Isystem::loadEncrypted() */
    virtual std::map<std::string, std::string> loadEncrypted();

    /** @sa ISystem::getEsn() */
    virtual const std::string getEsn() const;
    virtual void setEsn(std::string esn);

    /** @sa ISystem::getDeviceModel() */
    virtual bool getDeviceModel(std::string &deviceModel) const;

    /** @sa ISystem::getLanguage() */
    virtual const std::string getLanguage() const;

    /** @sa ISystem::getCapability(Capability&) */
    virtual Capability getCapability() const;

    /** @sa ISystem::getCertificationVersion() */
    virtual uint32_t getCertificationVersion() const;

    /** @sa ISystem::getSoftwareVersion() */
    virtual const std::string getSoftwareVersion() const;

    /** @sa ISystem::getFriendlyName() */
    virtual const std::string getFriendlyName() const;

    /** @sa ISystem::getAuthenticationType() */
    virtual AuthenticationType getAuthenticationType() const;

    virtual void setAuthenticationType(AuthenticationType authType);

    /** @sa ISystem::getVideoOutputResolution() */
    virtual void getVideoOutputResolution(uint32_t&, uint32_t&) const;

    /** @sa ISystem::notifyApplicationState() */
    virtual void notifyApplicationState(netflix::device::ISystem::ApplicationState);

    virtual std::map<std::string, std::string> getStartupLogTags();

    /** This function instructs the DPI to reseed the SSL entropy. */
    virtual void reseedSslEntropy();

    /** Get the platform preferred IP connectivity mode. */
    virtual IpConnectivityMode getIpConnectivityMode();

    /**
     * Return the device current view mode. This function provides a standard way for
     * the device bridge to get the device current view mode.
     *
     * @return the current view mode.
     */
    virtual ViewModeType getCurrentViewMode() const;

    /**
     * Return the device available view modes. This function provides a standard way for
     * the device bridge to get the device available view modes.
     *
     * @return the available view modes.
     */
    virtual std::vector<ViewModeType> getAvailableViewModes() const;

    /**
     * Set the device view mode. This function provides a standard way for the device bridge
     * to set the view mode. Since it could take several seconds for device to switch view mode,
     * device must call DeviceBridge::viewModeChanged() when the view mode switching is complete.
     * DeviceBridge::viewModeChanged() will in turn send an event to the listening UI.
     *
     * @param[in] view mode to set.
     */
    virtual void setViewMode(ViewModeType& viewMode);

    /** @sa ISystem::getVolumeControlType() */
    virtual VolumeControlType getVolumeControlType();

    /** @sa ISystem::getVolume() */
    virtual double getVolume();

    /** @sa ISystem:: setVolume() */
    virtual void setVolume(double volumeInPercent);

    /** @sa ISystem:: getCurrentMinimumVolumeStep()*/
    virtual double getCurrentMinimumVolumeStep();

    /** @sa ISystem:: isMuted() */
    virtual bool isMuted();

    /** @sa ISystem::setMute() */
    virtual void setMute(bool muteSetting);

    /* Get signatures of each in-process shared object */
    virtual Variant getSignatures();

    virtual Variant getSystemValue(const std::string& key);

    /**
     *  Get the size of the device persistent cache.For 12.4 PS3 will give the size of their secure store
     *  size is in bytes
     */
    virtual ullong getSecureStoreSize() { return 1024 * 1024; }

    /*
     * This may not be straightforward method to AgrippinaESManager. FileSystem's
     * video capability sub module need to work with ESManager, and set by this
     * method.
     */
    void AgrippinasetESManager(std::shared_ptr<esplayer::AgrippinaESManager> ESManager);

    /*
     * **************
     * Network System
     * **************
     */

    /*  Show the Network Connectivity info*/
    void dumpDNS(std::vector<std::string>dnslist);

    /*  Local ipconnectivity status*/
    IpConnectivityMode mIpConnectivity;

    virtual std::vector< std::string > getDNSList();
    virtual std::vector< struct ISystem::NetworkInterface > getNetworkInterfaces();

    /*  Network connectivity monitor : would invoke the listener if any network change been detected*/
    class NetworkMonitor : public Application::Timer
    {
    public:
        NetworkMonitor(FileSystem* sys)
            : Application::Timer(2000), //check status every 2 sec
              m_sys(sys)
        {
        }

        virtual std::string describe() const { return "DPI::NetworkMonitor"; }
        virtual void timerFired();
        bool readNetwork();
    private:
        FileSystem* m_sys;
        std::vector< struct ISystem::NetworkInterface > m_nif;
        std::vector<std::string> mDnsList;
    };

    //Network monitor members
    std::shared_ptr<NetworkMonitor>  mNwMon;

    void updateNetwork();

    /*
     *  End of network
     */


    /*******************************************************************************
     * System audio
     *******************************************************************************/
    std::shared_ptr<SystemAudioManager> mSystemAudioManager;

    /*******************************************************************************
     * Display and Output related
     *******************************************************************************/
    /*
     * Returns all supported digital output whether it is active or not.
     */
    virtual std::vector<struct VideoOutputInfo> getSupportedVideoOutput();

    /**
     * Returns all "active" digital output link status.
     *
     * When there is no link established, return empty vector
     *
     * When changed, device should notify via event mechanism defined by
     * EventDispather.
     */
    virtual std::vector<struct VideoOutputState> getActiveVideoOutput();

    /*  Output connectivity monitor */
    class AudioVideoOutputMonitor : public Application::Timer
    {
    public:
        AudioVideoOutputMonitor(FileSystem* sys)
            : Application::Timer(1000),
              m_sys(sys)
        {
        }

        virtual std::string describe() const { return "DPI::AudioVideoOutputMonitor"; }
        virtual void timerFired();
        bool readAudioVideoOutput();
    private:
        FileSystem* m_sys;
        std::vector<struct VideoOutputInfo> lastSupportedVideoOutputs;
        std::vector<struct VideoOutputState> lastActiveVideoOutputs;
    };

    std::shared_ptr<AudioVideoOutputMonitor>  mAudioVideoOutputMonitor;

    void updateAudioVideoOutput();

    void getAudioVideoOutputs(std::vector<struct VideoOutputInfo> & videoOutputs,
                              std::vector<struct VideoOutputState> & activeVideoOutputs);

    //--------------------------------------------------------------------------------

    void setTeeStorage(std::shared_ptr<TeeApiStorage> teeStoragePtr);

private:
    // RAM copy of the secure storage.
    std::map<std::string, std::string> mLastMap;

protected:
    std::string mEsn;        //!< ESN.

    std::string mDeviceModel;

    std::shared_ptr<EventDispatcher> mEventDispatcher;

    AuthenticationType mAuthType; //!< Authentication Type

    std::vector<ContentProfile> mVideoProfiles;
    std::vector<ContentProfile> mAudioProfiles;
    std::vector<ContentProfile> mAudioDecodeProfiles;
    std::vector<EaseProfile>    mAudioEaseProfiles;

    std::vector<struct VideoOutputInfo> mSupportedVideoOutput;
    std::vector<struct VideoOutputState> mActiveVideoOutput;

    friend class QADeviceBridge;
    friend class AgrippinaESManager;

    int32_t mQaBridgeIpConnectivityMode;

    VolumeControlType mVolumeControlType;

    bool mSupportTraceroute;

    std::shared_ptr<esplayer::AgrippinaESManager> mAgrippinaESManager;

    bool mSupport2DVideoTexture;

public:
    /*
     * Dolby Vision - static because we don't want to pass FileSystem to
     * VideoESPlayer for QABridge test
     */
    static bool sQaBridgeRenderDolbyVisionBL;
    static bool sQaBridgeRenderDolbyVisionEL;
    static std::string sQaBridgeDolbyFileDump;

private:
    std::string                 mEncryptedFile;    //!< Encrypted filename.

    bool mScreensaverOn;
    std::shared_ptr<TeeApiStorage> teeStoragePtr_;
};

/**
 * @class PresharedFileSystem FileSystem.h
 * @brief A file-based system implementation using pre-shared key
 *        authentication.
 */
class PresharedFileSystem : public FileSystem
{
public:
    /**
     * Create a new file-based system implementation using the
     * provided individualization data source and persistent store
     * locations.
     *
     * Individualization data files are plain-text files consisting of
     * three lines:
     * ESN
     * Kpe
     * Kph
     *
     * @param[in] idFile individualization data file.
     * @param[in] encryptedFile encrypted storage file location.
     */
    PresharedFileSystem(const std::string& idFile,
                        const std::string& pubkeyFile,
                        const std::string& encryptedFile);

    /** Destructor. */
    virtual ~PresharedFileSystem() {}
};

/**
 * @class ModelGroupFileSystem FileSystem.h
 * @brief A file-based system implementation using model group key
 *        authentication.
 */
class ModelGroupFileSystem : public FileSystem
{
public:
    /**
     * Create a new file-based system implementation using the
     * provided individualization data source and persistent store
     * locations.
     *
     * Individualization data files are plain-text files consisting of
     * three lines:
     * ESN
     * Kde
     * Kdh
     *
     * @param[in] idFile individualization data file.
     * @param[in] encryptedFile encrypted storage file location.
     */
    ModelGroupFileSystem(const std::string& idFile,
                         const std::string& pubkeyFile,
                         const std::string& encryptedFile);

    /** Destructor. */
    virtual ~ModelGroupFileSystem() {}
};

}} // namespace netflix::device

#endif
