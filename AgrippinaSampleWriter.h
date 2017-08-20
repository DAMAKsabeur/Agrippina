/*
 * (c) 1997-2016 Netflix, Inc.  All content herein is protected by
 * U.S. copyright and other applicable intellectual property laws and
 * may not be copied without the express permission of Netflix, Inc.,
 * which reserves all rights.  Reuse of any of this content for any
 * purpose without the permission of Netflix, Inc. is strictly
 * prohibited.
 */
#ifndef AGRIPIPINA_SAMPLEWRITER_H
#define AGRIPIPINA_SAMPLEWRITER_H

/** @file AgrippinaSampleWriter.h - Reference implementation of the
 * ISampleWriter interface.
 *
 * AgrippinaSampleWriter is the reference application's implementation of the
 * ISampleWriter interface (see IElementaryStreamPlayer.h). A device partner may
 * use or modify this header and accompanying .cpp as needed.
 *
 */

#include <nrd/IElementaryStreamPlayer.h>
#include <nrdbase/tr1.h>
#include <vector>

namespace netflix  {
namespace device {
namespace esplayer {

class AgrippinaSampleWriter : public ISampleWriter
{
public:
    AgrippinaSampleWriter(Format3D format3D, std::shared_ptr<IESPlayerCallback> callback);
    virtual ~AgrippinaSampleWriter();
    virtual Status initSample(const ISampleAttributes& sampleAttributes);
    virtual uint32_t write(const uint8_t *ptr, uint32_t size, uint32_t viewNum = 0);
    uint8_t *getBuffer(uint32_t viewNum = 0);

    // Returns sample size including prepended codec specific data.
    uint32_t getSampleSize(uint32_t viewNum = 0);
    const ISampleAttributes* getSampleAttributes();
    void reset();

private:
    const ISampleAttributes*  mSampleAttributes;
    Format3D                  mFormat3D;
    std::shared_ptr<IESPlayerCallback> mCallback;
    std::vector<uint8_t>      mBuffer;
    std::vector<uint32_t>     mSampleSizes;
    std::vector<uint32_t>     mBufferPositions;
    std::vector<uint32_t>     mViewOffsetInBuffer;

    void   calculateSampleSizesAndBufferPositions(const ISampleAttributes& attributes,
                                                  uint32_t& view0Size,
                                                  uint32_t& view1Size);
    Status reinitBufferIfNeeded(uint32_t size);
};

} // namespace esplayer
} // namespace device
} // namespace netflix

#endif /* AGRIPIPINA_SAMPLEWRITER_H */
