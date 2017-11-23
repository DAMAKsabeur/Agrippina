/*
 * (c) 1997-2016 Netflix, Inc.  All content herein is protected by
 * U.S. copyright and other applicable intellectual property laws and
 * may not be copied without the express permission of Netflix, Inc.,
 * which reserves all rights.  Reuse of any of this content for any
 * purpose without the permission of Netflix, Inc. is strictly
 * prohibited.
 */

#ifndef DEVICE_SYSTEM_CLOCK_H
#define DEVICE_SYSTEM_CLOCK_H
#include "NeroIntelCE4x00SystemClock.h"
using namespace std;

class NeroSystemClock
{
public:
    static Nero_error_t init();
    static Nero_error_t fini();
};



#endif




