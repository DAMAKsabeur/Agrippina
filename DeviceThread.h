/*
 * (c) 1997-2016 Netflix, Inc.  All content herein is protected by
 * U.S. copyright and other applicable intellectual property laws and
 * may not be copied without the express permission of Netflix, Inc.,
 * which reserves all rights.  Reuse of any of this content for any
 * purpose without the permission of Netflix, Inc. is strictly
 * prohibited.
 */
#ifndef DEVICE_DEVICETHREAD_H
#define DEVICE_DEVICETHREAD_H

/** @file DeviceThread.h - Defines a thread object used by the reference
 * implementations of the audio and video players. A device partner may use or
 * modify this header and accompanying .cpp file as needed.
 */

#include <nrdbase/Thread.h>
#include "AgrippinaESPlayer.h"

namespace netflix {
namespace device {
namespace esplayer {

/**
 * @class DeviceThread DeviceThread.h
 * @brief Wrapper of any thread that is running on the playback device.
 */
class DeviceThread : private Thread
{
public:
    //  typedef NFErr (AgrippinaPlaybackDevice::*Task)();
    typedef void (AgrippinaESPlayer::*Task)();

    /**
     * Constructor.
     *
     * @param[in] device the playback device instance.
     * @param[in] task the task (function) to be executed.
     * @param[in] priority the priority of thread.
     * @param[in] stackSize size of the stack in words.
     * @param[in] description informative description of thread.
     */
    DeviceThread(AgrippinaESPlayer& esplayer, Task task,
                 ThreadConfig *config);

    /**
     * Destructor.
     */
    virtual ~DeviceThread();

private:
    virtual void run();
    AgrippinaESPlayer& esPlayer_;
    Task task_;
};

} // namespace esplayer
} // namespace device
} // namespace netflix

#endif // DEVICE_DEVICETHREAD_H
