/*
 * (c) 1997-2016 Netflix, Inc.  All content herein is protected by
 * U.S. copyright and other applicable intellectual property laws and
 * may not be copied without the express permission of Netflix, Inc.,
 * which reserves all rights.  Reuse of any of this content for any
 * purpose without the permission of Netflix, Inc. is strictly
 * prohibited.
 */
#ifndef AGRIPIPINA_BUFFERMANAGER_H
#define AGRIPIPINA_BUFFERMANAGER_H

/** @file AgrippinaBufferManager.h - Reference implementation of the
 * IBufferManager interface.
 *
 * AgrippinaBufferManager is the reference application's implementation of the
 * IBufferManager interface. A device partner may use or modify this header and
 * accompanying .cpp as needed.
 *
 * In the reference application, AgrippinaDeviceLib, which is created once when the
 * application starts and persists until the application exits, creates one
 * instance of the buffer manager which persists until AgrippinaDeviceLib is
 * destroyed. IBufferManager::open() is called each time a movie is loaded for
 * playback and IBufferManager::close() is called when the movie is unloaded.
 *
 * The IBufferManager implementation must support release and re-use of buffer units
 * in arbitrary order. The IBufferManager interface does not need to be thread-safe.
 *
 * [This is a change from earlier versions in which alloc() and free() could be
 * called from different threads and where out-of-order release needed to be supported
 * but re-use of buffer units released out-of-order was not required]
 */

#include <nrd/IBufferManager.h>
#include <nrdbase/Mutex.h>
#include <list>

namespace netflix {
namespace device {

/**
 * @class AgrippinaBufferManager AgrippinaBufferManager.h
 * @brief Implementation of device buffer management functionality in native
 *        ansi C/C++ environment.
 */
class AgrippinaBufferManager : public IBufferManager
{
public:
    AgrippinaBufferManager(uint32_t audioPoolSize,
                        uint32_t videoPoolSize);

    virtual ~AgrippinaBufferManager();

    virtual NFErr open( uint32_t audioPoolUnitSize,
                        uint32_t videoPoolUnitSize);
    virtual void close();

    virtual uint32_t getBufferPoolSize(BufferDataType dataType);
    virtual uint32_t getBufferPoolFreeSpace(BufferDataType dataType);
    virtual NFErr alloc(BufferDataType dataType, uint32_t size, Buffer& buffer);
    virtual NFErr free(Buffer& buffer);

protected:
    virtual void setBufferPoolUnitSize(uint32_t dataType, uint32_t unitSize);

private:
    struct BufferPoolDesc
    {
        uint32_t bufferPoolReqSize_;
        uint32_t bufferPoolSize_;
        uint32_t bufferPoolFreeSpace_;
        uint32_t unitSize_;
        uint32_t head_;                 // First free block
        uint32_t tail_;                 // Last free block
        std::vector<uint32_t> free_;    // Index of next free block for all free blocks except the last
        unsigned char* pool_;
    };

    static const uint32_t numBufferDataTypes_ = 2;
    BufferPoolDesc bufferPoolDesc_[numBufferDataTypes_];
};

}} // namespace netflix::device

#endif // AGRIPIPINA_BUFFERMANAGER_H
