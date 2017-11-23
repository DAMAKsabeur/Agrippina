/*
 * NeroIntelCE4x00SystemClock.h
 *
 *  Created on: Nov 18, 2017
 *      Author: sabeur
 */

#ifndef NEROINTELCE4X00SYSTEMCLOCK_H_
#define NEROINTELCE4X00SYSTEMCLOCK_H_
#include "NeroIntelCE4x00Constants.h"
#include "NeroConstants.h"
extern "C"
{
#include "ismd_core.h"
#include "ismd_global_defs.h"
#include <sched.h>
}
using namespace std;

class NeroIntelCE4x00SystemClock
{
public:
    static Nero_error_t init();
    static Nero_error_t fini();
    static ismd_clock_t aquire();
    static ismd_time_t  GetTime();
    static Nero_error_t SetPauseTime(ismd_time_t time);
    static Nero_error_t SetBaseTime  (ismd_time_t time);
    static ismd_time_t  GetPauseTime();
    static ismd_time_t  GetBaseTime();
};



#endif /* NEROINTELCE4X00SYSTEMCLOCK_H_ */
