/*
 * (c) 1997-2016 Netflix, Inc.  All content herein is protected by
 * U.S. copyright and other applicable intellectual property laws and
 * may not be copied without the express permission of Netflix, Inc.,
 * which reserves all rights.  Reuse of any of this content for any
 * purpose without the permission of Netflix, Inc. is strictly
 * prohibited.
 */

#include "AgrippinaBufferManager.h"

#include <assert.h>

#include <new>
#include <nrdbase/ScopedMutex.h>
#include <nrd/AppLog.h>

using namespace netflix;
using namespace netflix::device;

AgrippinaBufferManager::AgrippinaBufferManager(uint32_t audioPoolSize,
                                         uint32_t videoPoolSize )
{
    NTRACE(TRACE_BUFFERMANAGER, "construct");

    uint32_t i;
    for(i = 0; i < numBufferDataTypes_; i ++)
    {
        // Remember the requested size (in bytes) of each buffer pool

        switch(i)
        {
        case AUDIO:
            bufferPoolDesc_[i].bufferPoolReqSize_ = audioPoolSize;
            break;
        case VIDEO:
            bufferPoolDesc_[i].bufferPoolReqSize_ = videoPoolSize;
            break;
        default:
            assert(false); // should never happen
        }

        // Reset all buffer pools

        bufferPoolDesc_[i].bufferPoolSize_ = 0;
        bufferPoolDesc_[i].bufferPoolFreeSpace_ = 0;
        bufferPoolDesc_[i].unitSize_ = 0;
        bufferPoolDesc_[i].head_ = 0;
        bufferPoolDesc_[i].tail_ = 0;
        bufferPoolDesc_[i].pool_ = NULL;
    }
}

AgrippinaBufferManager::~AgrippinaBufferManager()
{
    close();
}

NFErr AgrippinaBufferManager::open( uint32_t audioPoolUnitSize,
                                 uint32_t videoPoolUnitSize)
{
    NTRACE(TRACE_BUFFERMANAGER, "open audioPoolUnitSize %u, videoPoolUnitSize %u"
           ,audioPoolUnitSize, videoPoolUnitSize );

    bufferPoolDesc_[static_cast<uint32_t>( AUDIO )].unitSize_ = audioPoolUnitSize;
    bufferPoolDesc_[static_cast<uint32_t>( VIDEO )].unitSize_ = videoPoolUnitSize;

    uint32_t i;
    for(i = 0; i < numBufferDataTypes_; i ++)
    {
        // All buffer pools should have been properly reset

        assert(bufferPoolDesc_[i].bufferPoolSize_ == 0 &&
               bufferPoolDesc_[i].bufferPoolFreeSpace_ == 0 &&
               bufferPoolDesc_[i].head_ == 0 &&
               bufferPoolDesc_[i].tail_ == 0 &&
               bufferPoolDesc_[i].free_.empty() &&
               bufferPoolDesc_[i].pool_ == NULL );

        // If the requested size of a buffer pool is zero, the buffer pool is not
        // going to be used. Otherwise, create the buffer pool.

        if(bufferPoolDesc_[i].bufferPoolReqSize_ > 0)
        {
            assert( bufferPoolDesc_[i].unitSize_ != 0 );

            // Allocate memory for the buffer pool according to the requested size
            bufferPoolDesc_[i].pool_ =
                new (std::nothrow) unsigned char[bufferPoolDesc_[i].bufferPoolReqSize_];

            // Check for allocation failure.
            if(bufferPoolDesc_[i].pool_ == NULL)
            {
                Log::error(TRACE_BUFFERMANAGER, "AgrippinaBufferManager::open() Error: "
                           "allocation failure for AgrippinaBufferManager buffer pool %d."
                           " Desired size: %d", i, bufferPoolDesc_[i].bufferPoolReqSize_);

                // Clear any buffer allocations that may have succeeded before
                // this allocation failure.
                close();

                return NFErr_Bad;
            }

            // Set the buffer pool size to equal to the requested size
            bufferPoolDesc_[i].bufferPoolSize_ =
                bufferPoolDesc_[i].bufferPoolReqSize_;

            setBufferPoolUnitSize( i, bufferPoolDesc_[i].unitSize_ );

            NTRACE(TRACE_BUFFERMANAGER,
                   "open: created buffer pool %d = %p, size %d",
                   i, bufferPoolDesc_[i].pool_,
                   bufferPoolDesc_[i].bufferPoolSize_);
        }
    }

    return NFErr_OK;
}

void AgrippinaBufferManager::close()
{
    NTRACE(TRACE_BUFFERMANAGER, "close");
    uint32_t i;
    for(i = 0; i < numBufferDataTypes_; i ++)
    {
        // Reset all buffer pools and free the allocated memory (if any)

        bufferPoolDesc_[i].bufferPoolSize_ = 0;
        bufferPoolDesc_[i].bufferPoolFreeSpace_ = 0;
        bufferPoolDesc_[i].unitSize_ = 0;
        bufferPoolDesc_[i].head_ = 0;
        bufferPoolDesc_[i].tail_ = 0;
        bufferPoolDesc_[i].free_.clear();

        if(bufferPoolDesc_[i].pool_ != NULL)
        {
            NTRACE(TRACE_BUFFERMANAGER, "close: deleting buffer pool %d: %p",
                   i, bufferPoolDesc_[i].pool_);
            delete [] bufferPoolDesc_[i].pool_;
            bufferPoolDesc_[i].pool_ = NULL;
        }
    }
}

void AgrippinaBufferManager::setBufferPoolUnitSize(uint32_t dataType,
                                                uint32_t unitSize)
{
    NTRACE(TRACE_BUFFERMANAGER, "setBufferPoolUnitSize");

    uint32_t i = static_cast<uint32_t>(dataType);
    assert(i < numBufferDataTypes_);

    // Remember the unit size (in bytes) that is set
    bufferPoolDesc_[i].unitSize_ = unitSize;

    // When unit size is set, by design the buffer pool should be reset and
    // become totally vacant

    assert(unitSize != 0);
    assert(bufferPoolDesc_[i].bufferPoolReqSize_ / unitSize < 65535 );

    // If there is a fixed buffer unit size, make sure the buffer pool size is
    // an integral multiple of the unit size (and smaller than the requested
    // buffer pool size of course)

    bufferPoolDesc_[i].bufferPoolSize_ =
        (bufferPoolDesc_[i].bufferPoolReqSize_ / unitSize) * unitSize;

    bufferPoolDesc_[i].bufferPoolFreeSpace_ = bufferPoolDesc_[i].bufferPoolSize_;

    bufferPoolDesc_[i].head_ = 0;


    bufferPoolDesc_[i].free_.clear();

    bufferPoolDesc_[i].free_.reserve( bufferPoolDesc_[i].bufferPoolSize_ / unitSize );

    for( uint32_t index = 0; index < bufferPoolDesc_[i].bufferPoolSize_ / unitSize; ++index )
        bufferPoolDesc_[i].free_.push_back( index + 1 );

    bufferPoolDesc_[i].tail_ = bufferPoolDesc_[i].bufferPoolSize_ / unitSize - 1;
}

uint32_t AgrippinaBufferManager::getBufferPoolSize(BufferDataType dataType)
{
    uint32_t i = static_cast<uint32_t>(dataType);
    assert(i < numBufferDataTypes_);

    // NOTE: by design, the results of buffer pool size enquiry before and after
    // "setBufferPoolUnitSize()" could be different. But the difference should be
    // small enough to be ignored.

    return bufferPoolDesc_[i].bufferPoolSize_;
}

uint32_t AgrippinaBufferManager::getBufferPoolFreeSpace(BufferDataType dataType)
{
    uint32_t i = static_cast<uint32_t>(dataType);
    assert(i < numBufferDataTypes_);

    assert(bufferPoolDesc_[i].bufferPoolSize_ > 0); // pool must have been created

    return bufferPoolDesc_[i].bufferPoolFreeSpace_;
}

NFErr AgrippinaBufferManager::alloc(BufferDataType dataType,
                                 uint32_t size,
                                 Buffer& buffer)
{
    assert(size > 0); // allocation of zero bytes is unexpected

    uint32_t const i = dataType;
    assert(i < numBufferDataTypes_);

    assert(bufferPoolDesc_[i].bufferPoolSize_ > 0 && // pool must have been created
           bufferPoolDesc_[i].pool_ != NULL);        // pool must have been created

    //NTRACE(TRACE_BUFFERMANAGER, "alloc: %d, free=%p", size, bufferPoolDesc_[i].free_);

    // If the buffer unit size is not set (zero), the number of bytes to allocate is
    // not restricted. Otherwise, the number of bytes to allocate must be equal to
    // the buffer unit size

    assert(bufferPoolDesc_[i].unitSize_ == size);

    if ( bufferPoolDesc_[i].bufferPoolFreeSpace_ < size )
        return NFErr_Pending;

    // Initialize the buffer object descriptor structure


    buffer.bufferDataType_ = dataType;
    buffer.private_ = NULL; // not used
    buffer.byteBuffer_ = bufferPoolDesc_[i].pool_ + bufferPoolDesc_[i].head_ * bufferPoolDesc_[i].unitSize_;
    buffer.capacity_ = size;
    buffer.allocSeq_ = 0;   // not used

    bufferPoolDesc_[i].bufferPoolFreeSpace_ -= size;
    bufferPoolDesc_[i].head_ = bufferPoolDesc_[i].free_[ bufferPoolDesc_[i].head_ ];

    return NFErr_OK;
}

NFErr AgrippinaBufferManager::free(Buffer& buffer)
{
    uint32_t i = static_cast<uint32_t>(buffer.bufferDataType_);

    //NTRACE(TRACE_BUFFERMANAGER, "free: %p, free=%p", buffer.byteBuffer_, bufferPoolDesc_[i].free_);

    assert( buffer.byteBuffer_ >= bufferPoolDesc_[i].pool_ );
    assert( buffer.byteBuffer_ < bufferPoolDesc_[i].pool_ + bufferPoolDesc_[i].bufferPoolSize_ );
    assert( ( buffer.byteBuffer_ - bufferPoolDesc_[i].pool_ ) % bufferPoolDesc_[i].unitSize_ == 0 );
    assert( buffer.capacity_ == bufferPoolDesc_[i].unitSize_ );

    uint32_t const index = ( buffer.byteBuffer_ - bufferPoolDesc_[i].pool_ ) / bufferPoolDesc_[i].unitSize_;

    if ( bufferPoolDesc_[i].bufferPoolFreeSpace_ == 0 )
    {
        bufferPoolDesc_[i].head_ = index;
    }
    else
    {
        bufferPoolDesc_[i].free_[ bufferPoolDesc_[i].tail_ ] = index;
    }

    bufferPoolDesc_[i].tail_ = index;

    bufferPoolDesc_[i].bufferPoolFreeSpace_ += buffer.capacity_;

    return NFErr_OK;
}
