/*
 * NeroIntelCE4x00VideoDecoder.h
 *
 *  Created on: Aug 12, 2017
 *      Author: g360476
 */

#ifndef NEROINTELCE4x00_VIDEO_DECODER_H_
#define NEROINTELCE4x00_VIDEO_DECODER_H_
#include "NeroConstants.h"
#include "NeroBlockingQueue.h"
#include "NeroIntelCE4x00Constants.h"
#include "NeroIntelCE4x00SystemClock.h"
/*#include "Observable.h"*/
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
// Video Player Settings
#define REN_SCREEN_PAR_X              1; // Screen Pixel Aspect Ratio
#define REN_SCREEN_PAR_Y              1; // Screen Pixel Aspect Ratio

#define VIDEO_REN_SCREEN_WIDTH              720;
#define VIDEO_REN_SCREEN_HEIGHT             480;

#define PIXEL_FORMAT_NONE            0;
#define VIDEO_REN_MIN_FRAME_BUFFERS  2;
class NeroIntelCE4x00VideoDecoder /* : public Observable*/ {
	
public:

NeroIntelCE4x00VideoDecoder();
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
Nero_error_t   NeroVideoDecoderEventWait(NeroEvents_t *event);
/*********************/
private:
/* private functions */
Nero_error_t  NeroIntelCE4x00VideoDecoder_EventSubscribe();
Nero_error_t  NeroIntelCE4x00VideoDecoder_EventUnSubscribe();
ismd_clock_t NeroIntelCE4x00VideoDecoderCreateInternalClock();
Nero_error_t NeroIntelCE4x00VideoDecoderInvalidateHandles();
Nero_error_t NeroIntelCE4x00VideoDecoderSend_new_segment();
ismd_codec_type_t NeroIntelCE4x00VideoDecoder_NERO2ISMD_codeRemap(Nero_video_codec_t NeroVideoAlgo);

/* private variables */

/* graphical data */
gdl_boolean_t              is_scaling_enabled;
gdl_plane_id_t             layer;
gdl_display_info_t         display;
gdl_rectangle_t            rect;
uint8_t                    alpha;

/* video data */
os_thread_t VideoDecoderTask ;
NeroDecoderState_t DecoderState;
/* ismd handles*/
ismd_time_t original_base_time;
ismd_time_t orginal_clock_time;
ismd_time_t curr_clock;
ismd_time_t  curr_time;
NeroDecoderState_t     VideoDecoderState;
bool internal_clk;
/* video device handles */
ismd_dev_t viddec_handle;
ismd_dev_t vidpproc_handle;
ismd_dev_t vidrend_handle;
/* video ports handles */
ismd_port_handle_t viddec_input;
ismd_port_handle_t viddec_output;
ismd_port_handle_t vidpproc_input;
ismd_port_handle_t vidpproc_output;
ismd_port_handle_t vidrend_input;
ismd_buffer_handle_t     buffer;
ismd_clock_t    clock_handle;
ismd_event_t                Nero_Events[NERO_EVENT_LAST];
};

#endif /* NEROINTELCE4x00_VIDEO_DECODER_H_ */
