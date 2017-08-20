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

class NeroIntelCE4x00VideoDecoder  : public Observable {
	
public:

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
};

#endif /* NEROINTELCE4x00_VIDEO_DECODER_H_ */
