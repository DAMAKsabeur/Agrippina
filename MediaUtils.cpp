
/*
 * (c) 1997-2016 Netflix, Inc.  All content herein is protected by
 * U.S. copyright and other applicable intellectual property laws and
 * may not be copied without the express permission of Netflix, Inc.,
 * which reserves all rights.  Reuse of any of this content for any
 * purpose without the permission of Netflix, Inc. is strictly
 * prohibited.
 */
#include <vector>
#include <string>
#include <assert.h>
#include <nrdbase/Log.h>
#include "Common.h"
#include "MediaUtils.h"

using namespace netflix::device::esplayer;
using namespace netflix;


bool MediaUtils::createADTSHeader(std::vector<uint8_t>& header, const uint8_t *codecSpecificData,
     uint32_t codecSpecificDataLen)
{
    // parse AudioSpecificConfig() (in ISO/IEC 14496-3 Table 1.15) and
    // use info to build adts header (ISO/IEC 14496-3 Table 1.A.6 & Table 1.A.7)
    assert(codecSpecificDataLen >= 2);
    if(codecSpecificDataLen < 2)
    {
        Log::warn(TRACE_MEDIAPLAYBACK, "MediaUtils::createADTSHeader: codecSpecificDataLen < 2.");
        return false;
    }

    uint8_t samplingFrequencyIndex = ((codecSpecificData[0] & 0x7) << 1) | (codecSpecificData[1] >> 7);

    if (samplingFrequencyIndex != 6) { // support only 48KHz
        Log::warn(TRACE_MEDIAPLAYBACK, "MediaUtils::createADTSHeader: Unsupported sampling frequency index");
        return false;
    }

    uint8_t channelConfiguration = (codecSpecificData[1] >> 3) & 0xf;
    uint8_t audioObjectType	= (codecSpecificData[0] >> 3) & 0x1f;

    uint8_t ID = 1; // start with MPEG-2

    if (audioObjectType == 5) {
        ID = 0; // MPEG-4
    } else if (audioObjectType == 2) {
        // parse further to see if there's an extensionAudioObjectType of 5,
        //   if so, overwrite ID with 0

        // Skip 3 bits of GASpecificConfig()

        if (codecSpecificDataLen >= 4) {
            // if (extensionAudioObjectType != 5 && bits_to_decode() >= 16)
            uint32_t syncExtensionType = codecSpecificData[2] << 3 | codecSpecificData[3] >> 5;
            if (syncExtensionType == 0x2b7) {
                uint8_t extensionAudioObjectType = codecSpecificData[3] & 0x1f;
                if (extensionAudioObjectType == 5) {
                    ID = 0;
                } else {
                    Log::warn(TRACE_MEDIAPLAYBACK, "MediaUtils::createADTSHeader:invalid "
                                 "extensionAudioObjectType %d", extensionAudioObjectType);
                    return false;
                }
            }
        }
    } else {
        Log::warn(TRACE_MEDIAPLAYBACK, "MediaUtils::createADTSHeader:invalid "
                     "audioObjectType %d", audioObjectType);
        return false;
    }

    /*
     * build adts header
     *
     * syncword: 0xfff
     * ID: <varies>
     * layer: 0
     * protection_absent: 1
     * profile_ObjectType: 1
     * sampling_frequency_index: <varies>
     * private_bit: 0
     * channel_configuration: <varies>
     * original_copy: 0
     * home: 0
     * copyright_identification_bit: 0
     * copyright_identification_start: 0
     * aac_frame_length: <varies>
     * adts_buffer_fullness: 0x7ff
     * number_of_raw_data_blocks_in_frame: 0
     */
    header.clear();
    header.push_back(0xff);
    header.push_back(ID ? 0xf9 : 0xf1);
    header.push_back(
        0x40 | (samplingFrequencyIndex << 2) | (channelConfiguration >> 2));
    header.push_back((channelConfiguration & 0x3) << 6);
    header.push_back(0);
    header.push_back(0x1f);
    header.push_back(0xfc);

    return true;
}


bool MediaUtils::updateADTSHeaderSizeBytes(std::vector<uint8_t>& header, uint32_t auSize)
{
    assert(header.size() > 5);
    if(header.size() <= 5)
    {
        Log::warn(TRACE_MEDIAPLAYBACK, "MediaUtils::updateADTSHeaderSizeBytes: header "
                  "size %zu <= 5 bytes", header.size());
        return false;
    }
    uint8_t *buf = &(header[0]);

    buf[3] = (buf[3] & 0xfc) | (auSize >> 11);
    buf[4] = (auSize >> 3) & 0xff;
    buf[5] = (buf[5] & 0x1f) | ((auSize & 0x7) << 5);

    return true;
}



uint32_t MediaUtils::getFourCCFromCodecParam(const std::string& codecParam)
{
        // Set mCodecParam string
    if(codecParam.find("mp4a") != std::string::npos)
    {
        return 0x1610; // MEDIASUBTYPE_MPEG_HEAAC
    } else if(codecParam.find("ec-3") != std::string::npos)
    {
        return 0xeac3;  // DOLBY DIGITAL PLUS
    } else if (codecParam.compare("ovrb") == 0)
    {
        return  0x6771; // OGG VORBIS 3 PLUS
    } else if (codecParam.find("avc1") != std::string::npos)
    {
        return _FOURCC_LE_('a','v','c','1');
    } else if (codecParam.find("hev1") != std::string::npos)
    {
        return _FOURCC_LE_('h','e','v','1');
    } else if (codecParam.find("hvc1") != std::string::npos)
    {
        return _FOURCC_LE_('h','v','c','1');
    } else if (codecParam.find("hdr10") != std::string::npos)
    {
        return _FOURCC_LE_('h','v','c','1');
    } else if (codecParam.find("dvhe") != std::string::npos)
    {
        return _FOURCC_LE_('d','v','h','e');
    } else if (codecParam.find("vp09") != std::string::npos)
    {
        return _FOURCC_LE_('v','p','0','9');
    }
    else if (codecParam.find("h264") != std::string::npos)
    {
        return _FOURCC_LE_('h','2','6','4');
    }
    else if (codecParam.find("mpg4") != std::string::npos)
    {
        return _FOURCC_LE_('m','p','g','4');
    }
    else if (codecParam.find("jpeg") != std::string::npos)
    {
        return _FOURCC_LE_('j','p','e','g');
    }
    Log::error(TRACE_MEDIAPLAYBACK, "AgrippinaPlaybackDevice::getFourCCFromCodecParam(): "
               "unknown codec param string: %s", codecParam.c_str());
    return 0;
}


MediaUtils::FrameRate MediaUtils::getCE3FrameRate(uint32_t frameRateValue, uint32_t frameRateScale)
{
    double fps = static_cast<double>(frameRateValue) /  frameRateScale;
    static const int numCE3FrameRates = 5;
    static const double decision_thresholds[numCE3FrameRates-1] = {23.988, 24.5, 27.485, 29.985};

    if(fps < decision_thresholds[0])
    {
        return VIDEO_FRAMERATE_23_976;
    } else if(fps < decision_thresholds[1]) {
        return VIDEO_FRAMERATE_24;
    } else if (fps < decision_thresholds[2]) {
        return VIDEO_FRAMERATE_25;
    } else if (fps < decision_thresholds[3]) {
        return VIDEO_FRAMERATE_29_97;
    }
    return VIDEO_FRAMERATE_30;
}
