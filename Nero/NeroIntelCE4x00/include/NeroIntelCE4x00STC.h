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
#include "NeroAudioDecoder.h"
#include "NeroVideoDecoder.h"
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
NeroIntelCE4x00STC(Nero_stc_type_t SyncMode);
~NeroIntelCE4x00STC();
Nero_error_t ConncectSources(NeroAudioDecoder* NeroAudioDecoder_ptr, NeroVideoDecoder* NeroVideoDecoder_ptr);
Nero_error_t DisconncectSources();
NeroAudioDecoder* GetAudioDecoder ();
NeroVideoDecoder* GetVideoDecoder ();
Nero_error_t SetSyncMode (Nero_stc_type_t SyncMode);
Nero_stc_type_t GetSyncMode ();
uint64_t GetLastSyncPts();
uint64_t last_syc_pts;
private:

os_thread_t stc_task ;
NeroAudioDecoder* m_NeroAudioDecoder_ptr;
NeroVideoDecoder* m_NeroVideoDecoder_ptr;
Nero_stc_type_t  m_SyncMode;
};

#endif /* NEROINTELCE4X00_STC_H_ */
