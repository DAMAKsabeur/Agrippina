/*
 * NeroIntelCE4x00STC.h
 *
 *  Created on: Aug 12, 2017
 *      Author: g360476
 */

#ifndef NERO_STC_H_
#define NERO_STC_H_
#include "NeroConstants.h"
//#include "NeroIntelCE4x00Constants.h"
#include "NeroIntelCE4x00STC.h"

/* define macros */

using namespace std;

class NeroSTC{
public:
NeroSTC();
NeroSTC (uint8_t STCtype);
~NeroSTC();

Nero_error_t NeroSTCSetPlayBack();
nero_clock_handle_t NeroSTCGetClock();
Nero_error_t NeroSTCSetBaseTime(uint64_t time);
uint64_t NeroSTCGetBaseTime();
Nero_stc_type_t NeroSTCGetType();
Nero_error_t NeroSTCSetType(Nero_stc_type_t stc_type);
private:
/* private functions */
NeroSTC_class* m_NeroSTC;
/* private variables */

};

#endif /* NERO_STC_H_ */
