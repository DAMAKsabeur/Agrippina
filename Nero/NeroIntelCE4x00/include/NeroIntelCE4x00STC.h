/*
 * NeroIntelCE4x00STC.h
 *
 *  Created on: Aug 12, 2017
 *      Author: g360476
 */

#ifndef NEROINTELCE4X00_STC_H_
#define NEROINTELCE4X00_STC_H_
#include "NeroConstants.h"
#include "NeroIntelCE4x00Constants.h"

extern "C"
{
#include "ismd_core.h"
#include "ismd_global_defs.h"
#include <sched.h>
}

/* define macros */
#define NERO_AV_PLAYBACk_CLK 0x01

using namespace std;

class NeroIntelCE4x00STC{
public:

NeroIntelCE4x00STC();
NeroIntelCE4x00STC(uint8_t type);
~NeroIntelCE4x00STC();

Nero_error_t NeroSTCSetPlayBack();
nero_clock_handle_t NeroSTCGetClock();
Nero_error_t NeroSTCSetBaseTime(uint64_t time);
uint64_t NeroSTCGetBaseTime();
Nero_stc_type_t NeroSTCGetType();
Nero_error_t NeroSTCSetType(Nero_stc_type_t stc_type);
private:
/* private functions */

Nero_error_t NeroIntelCE4x00STCInvalidateHandles();

/* private variables */
Nero_stc_type_t Nero_stc_type;
ismd_clock_t clock_handle;
ismd_time_t  basetime;
};

#endif /* NEROINTELCE4X00_STC_H_ */
