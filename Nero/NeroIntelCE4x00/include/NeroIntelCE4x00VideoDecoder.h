/*
 * NeroIntelCE4x00VideoDecoder.h
 *
 *  Created on: Aug 12, 2017
 *      Author: g360476
 */

#ifndef NEROINTELCE4x00_VIDEO_DECODER_H_
#define NEROINTELCE4x00_VIDEO_DECODER_H_
#include "NeroConstants.h"

#include "NeroIntelCE4x00Constants.h"
#include "NeroSTC.h"
#include "Observable.h"
extern "C"
{
#include "osal.h"
#include "ismd_core.h"
#include "ismd_global_defs.h"
#include "ismd_viddec.h"
#include "ismd_vidpproc.h"
#include "ismd_vidrend.h"
#include "ismd_vidsink.h"
#include "libgdl.h"
#include <sched.h>
}

using namespace std;

#define PTS_START_VALUE 0
#define INPUT_BUF_SIZE (32*1024) /* inject up to 32K of Video data per buffer*/

//*** VIDDEC ***
#define VIDDEC_NB_EVENT_TO_MONITOR 0x04

//*** VIDREND ***
#define VIDREND_NB_EVENT_TO_MONITOR 0x07

#define EVENT_TIMEOUT         0x0A
#define VIDEO_NB_EVENT_TO_MONITOR    (VIDDEC_NB_EVENT_TO_MONITOR+VIDREND_NB_EVENT_TO_MONITOR)
class NeroIntelCE4x00VideoDecoder  : public Observable {
	
public:
bool vid_evt_handler_thread_exit;
ismd_event_t ismd_vid_evt_tab[VIDEO_NB_EVENT_TO_MONITOR];
ismd_viddec_event_t viddec_evt_to_monitor [VIDDEC_NB_EVENT_TO_MONITOR];
ismd_vidrend_event_type_t vidrend_evt_to_monitor[VIDREND_NB_EVENT_TO_MONITOR];
ismd_event_t triggered_event;
pthread_mutex_t mutex_stock;
int nb_vid_evt;
NeroIntelCE4x00VideoDecoder();
NeroIntelCE4x00VideoDecoder(NeroSTC* NeroSTC_ptr);
~NeroIntelCE4x00VideoDecoder();

Nero_error_t   NeroVideoDecoderSetupPlane();
Nero_error_t   NeroVideoDecoderResizeLayer(size_t w,size_t h,size_t x,size_t y);

Nero_error_t   NeroVideoDecoderInit (Nero_video_codec_t NeroVideoAlgo);
Nero_error_t   NeroVideoDecoderReconfigure(Nero_video_codec_t NeroVideoAlgo);
Nero_error_t   NeroVideoDecoderUnInit();
Nero_error_t   NeroVideoDecoderFlush();
Nero_error_t   NeroVideoDecoderStop();
Nero_error_t   NeroVideoDecoderClose();
Nero_error_t   NeroVideoDecoderPause();
Nero_error_t   NeroVideoDecoderPlay();
Nero_error_t   NeroVideoDecoderFeed (uint8_t* payload, size_t payload_size, uint64_t nrd_pts, bool discontinuity);
uint64_t NeroVideoDecoderGetLastPts();
NeroDecoderState_t NeroVideoDecoderGetState();

/** for testing to be used for PES injection */
int            NeroVideoDecoderGetport();
/** to be checked observer pattern*/
void Change(int valeur);
Info Statut(void) const;
/*********************/
private:
/* private functions */
ismd_clock_t NeroIntelCE4x00VideoDecoderCreateInternalClock();
Nero_error_t NeroIntelCE4x00VideoDecoderInvalidateHandles();
Nero_error_t NeroIntelCE4x00VideoDecoderSend_new_segment();
ismd_codec_type_t NeroIntelCE4x00VideoDecoder_NERO2ISMD_codeRemap(Nero_video_codec_t NeroVideoAlgo);

ismd_result_t NeroIntelCE4x00VideoDecoder_subscribe_events();
ismd_result_t NeroIntelCE4x00VideoDecoder_unsubscribe_events();

/* private variables */

NeroDecoderState_t DecoderState;
/* ismd handles*/
ismd_time_t original_base_time;
ismd_time_t orginal_clock_time;
ismd_time_t curr_clock;
bool internal_clk;
ismd_dev_t viddec_handle;
ismd_dev_t vidpproc_handle;
ismd_dev_t vidrend_handle;
ismd_vidsink_dev_t vidsink_handle;
ismd_port_handle_t viddec_input_port_handle;
ismd_port_handle_t viddec_output_port_handle;
ismd_port_handle_t vidpproc_input_port_handle;
gdl_plane_id_t layer;

ismd_clock_t    clock_handle;
NeroSTC* stc;
os_thread_t vid_evt_handler_thread;
};

#endif /* NEROINTELCE4x00_VIDEO_DECODER_H_ */
