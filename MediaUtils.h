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

/* @file MediaUtils.h - Contains a set of static media utility functions wrapped
 * up in a MediaUtils class.  The reference implementation uses these methods
 * and they are available for use in partner implementations.  A device partner
 * may use or modify this header and accompanying .cpp file as needed.
 */

namespace netflix  {
namespace device {
namespace esplayer {

    class MediaUtils
    {
  public:

        /**
         * Creates an AAC ADTS header from  esds mp4a decoder configuration bytes.
         *
         * @param[out] header: 7-byte ADTS header that needs to be prepended to AAC
         *             samples for some AAC decoders.
         * @param[in]  codecSpecificData: points to buffer containing mp4a decoder configuration bytes.
         * @param[in]  codecSpecificDataLen: size of the mp4a decoder configuration bytes.
         *
         * @return true on success, false otherwise.
         */
        static bool createADTSHeader(std::vector<uint8_t>& header, const uint8_t *codecSpecificData,
                                     uint32_t codecSpecificDataLen);


        /**
         * Part of the ADTS header specifies the size of the sample. This method
         * takes an existing AAC ADTS header and updates the size bytes.
         *
         * @param[in/out] header: a vector containing ADTS header bytes.
         *
         * @param[in] sampleSize: specifies the size of the sample that the ADTS
         * header will be prepended to.  The size should include the size of the
         * ADTS header.
         *
         * @return true on success, false otherwise.
         */
        static bool updateADTSHeaderSizeBytes(std::vector<uint8_t>& header, uint32_t sampleSize);


        /**
         * Converts commonly used "Codecs Parameter" strings as in
         * http://tools.ietf.org/search/draft-gellens-mime-bucket-bis-09 (see
         * IElementaryStreamPlayer.h for details) to fourCC codes.
         *
         * @param[in] codecParam: "Codecs Parameter" string.
         *
         * @return fourCC code.
         */
        static uint32_t getFourCCFromCodecParam(const std::string& codecParam);

        enum FrameRate {
            VIDEO_FRAMERATE_23_976,
            VIDEO_FRAMERATE_24,
            VIDEO_FRAMERATE_25,
            VIDEO_FRAMERATE_29_97,
            VIDEO_FRAMERATE_30
        };


        /**
         * Converts framerate expressed as a rational to one of the five
         * discrete framerates used in CE3 by rounds the framerate rational to
         * the closest CE3 framerate.
         *
         * @param[in] frameRateValue - the numerator in the rational frame rate
         *                             expression to be converted.
         * @param[in] frameRateScale - the denominator in the rational frame
         *                             rate expression to be converted.
         *
         * @return a FrameRate enum value.
         */

        static FrameRate getCE3FrameRate(uint32_t frameRateValue, uint32_t frameRateScale);


    private:
        MediaUtils();
    };


} // namespace esplayer
} // namespace device
} // namespace netflix

