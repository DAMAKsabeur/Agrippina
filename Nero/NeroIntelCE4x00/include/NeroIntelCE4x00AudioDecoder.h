#ifndef NEROINTELCE4x00_AUDIO_DECODER_H_
#define NEROINTELCE4x00_AUDIO_DECODER_H_
#include "NeroConstants.h"
#include "NeroIntelCE4x00Constants.h"
#include "NeroIntelCE4x00AudioProcessor.h"
#include "NeroBlockingQueue.h"
#include "pthread.h"          // std::queue
#include "NeroIntelCE4x00SystemClock.h"
/*#include "Observable.h"*/
extern "C"
{
#include "osal.h"
#include "ismd_core.h"
#include "ismd_global_defs.h"
#include "ismd_audio_defs.h"
#include "ismd_audio.h"
#include "psi_handler.h"
#include <sched.h>
}
/* define macros */

using namespace std;

#define PTS_START_VALUE 0
#define INPUT_BUF_SIZE (32*1024) /* inject up to 32K of audio data per buffer*/
#define AUDIO_NB_EVENT_TO_MONITOR   0x03
#define EVENT_TIMEOUT         10
class NeroIntelCE4x00AudioDecoder /* : public Observable*/{

public:

/*bool aud_evt_handler_thread_exit;
ismd_event_t ismd_aud_evt_tab[AUDIO_NB_EVENT_TO_MONITOR];
ismd_audio_notification_t aud_evt_to_monitor [AUDIO_NB_EVENT_TO_MONITOR];*/

NeroIntelCE4x00AudioDecoder();
~NeroIntelCE4x00AudioDecoder();

Nero_error_t   NeroAudioDecoderInit (Nero_audio_codec_t NeroAudioAlgo);
Nero_error_t   NeroAudioDecoderReconfigure(Nero_audio_codec_t NeroAudioAlgo);
Nero_error_t   NeroAudioDecoderUnInit();
Nero_error_t   NeroAudioDecoderFlush();
Nero_error_t   NeroAudioDecoderStop();
Nero_error_t   NeroAudioDecoderClose();
Nero_error_t   NeroAudioDecoderPause();
Nero_error_t   NeroAudioDecoderPlay();
Nero_error_t   NeroAudioDecoderFeed (uint8_t* payload, size_t payload_size, uint64_t nrd_pts, bool discontinuity);
uint64_t       NeroAudioDecoderGetLastPts();
NeroDecoderState_t NeroAudioDecoderGetState();
/** for testing to be used for PES injection */
int            NeroAudioDecoderGetport();
Nero_error_t   NeroAudioDecoderEventWait(NeroEvents_t *event);


/*********************/
private:
/* private functions */

ismd_clock_t NeroIntelCE4x00AudioDecoderCreateInternalClock();
Nero_error_t NeroIntelCE4x00AudioDecoderInvalidateHandles();
/*Nero_error_t NeroIntelCE4x00AudioDecoderSend_new_segment();*/
ismd_audio_format_t NeroIntelCE4x00AudioDecoder_NERO2ISMD_codeRemap(Nero_audio_codec_t NeroAudioAlgo);
Nero_error_t  NeroIntelCE4x00AudioDecoder_EventSubscribe();
Nero_error_t  NeroIntelCE4x00AudioDecoder_EventUnSubscribe();

/*ismd_result_t NeroIntelCE4x00AudioDecoder_subscribe_events();
ismd_result_t NeroIntelCE4x00AudioDecoder_unsubscribe_events();*/

/* private variables */
os_thread_t AudioDecoderTask ;
ismd_time_t pause_time;
ismd_time_t current_time;
ismd_time_t base_time;

NeroDecoderState_t     AudioDecoderState;
ismd_audio_format_t    audio_algo;
ismd_dev_t             audio_handle;
ismd_port_handle_t     audio_input_port_handle;
ismd_audio_processor_t audio_processor;
ismd_audio_output_t    audio_output_handle;
ismd_clock_t           clock_handle;
ismd_event_t           Nero_Events[NERO_EVENT_LAST];
/*os_thread_t aud_evt_handler_thread;*/

//thread evt_thread;
};
#endif /*NEROINTELCE4x00_AUDIO_DECODER_H_*/
