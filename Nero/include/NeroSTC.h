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
#include "NeroAudioDecoder.h"
#include "NeroVideoDecoder.h"
/* define macros */

using namespace std;

class NeroSTC{
public:
NeroSTC();
NeroSTC (Nero_stc_type_t SyncMode);
~NeroSTC();
Nero_error_t ConncectSources(NeroAudioDecoder* NeroAudioDecoder_ptr, NeroVideoDecoder* NeroVideoDecoder_ptr);
Nero_error_t DisconncectSources();
uint64_t GetLastSyncPts();
private:
/* private functions */
NeroSTC_class* m_NeroSTC;
/* private variables */

};

#endif /* NERO_STC_H_ */
