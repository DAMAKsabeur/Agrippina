#ifndef NERODEMUX_H_
#define NERODEMUX_H_
#include "../__NeroVideoDecoder.h"
#include "NeroConstants.h"
#include "NeroIntelCE4x00Constants.h"
#include "NeroClock.h"
#include "NeroAudioDecoder.h"
extern "C"
{
#include "osal.h"
#include "ismd_core.h"
#include "ismd_global_defs.h"
#include "ismd_demux.h"
#include "psi_handler.h"
#include <sched.h>
}
using namespace std;
#define INPUT_BUF_SIZE (32*1024) /* inject up to 32K of audio data per buffer*/
class NeroDemux{
public:
NeroDemux();
NeroDemux(nero_clock_handle_t clk);
~NeroDemux();
Nero_error_t NeroDemuxInit (uint16_t audio_pid, uint16_t video_pid, uint16_t pcr_pid,ismd_port_handle_t audio_input_port,ismd_port_handle_t video_input_port);
Nero_error_t NeroDemuxReconfigure(uint16_t audio_pid, uint16_t video_pid, uint16_t pcr_pid);
Nero_error_t NeroDemuxUnInit();
Nero_error_t NeroDemuxFlush();
Nero_error_t NeroDemuxStop();
Nero_error_t NeroDemuxPause();
Nero_error_t NeroDemuxPlay();
nero_time_t  NeroDemuxFeed(char* buff , size_t size);
NeroDecoderState_t NeroDemuxGetState();
Nero_error_t NeroDemuxSetState(NeroDecoderState_t NeroDemuxSetState);
nero_time_t NeroDemuxGetPauseTime();
private:
/* private functions */
ismd_result_t NeroIntelCE4x00DemuxInvalidateHandles();
ismd_result_t NeroIntelCE4x00DemuxSend_new_segment();
ismd_result_t NeroIntelCE4x00DemuxInit(uint16_t audio_pid, uint16_t video_pid, uint16_t pcr_pid,ismd_port_handle_t audio_input_port,ismd_port_handle_t video_input_port);
ismd_result_t NeroIntelCE4x00DemuxReconfigure(uint16_t audio_pid, uint16_t video_pid, uint16_t pcr_pid);
ismd_result_t NeroIntelCE4x00DemuxUnInit();
ismd_result_t NeroIntelCE4x00DemuxFlush();
ismd_result_t NeroIntelCE4x00DemuxStop();
ismd_result_t NeroIntelCE4x00DemuxPause();
ismd_result_t NeroIntelCE4x00DemuxPlay();
ismd_result_t NeroIntelCE4x00DemuxClose();
ismd_result_t NeroIntelCE4x00DemuxFeed(char* buff , size_t size);
ismd_time_t NeroIntelCE4x00DemuxGetPauseTime();
NeroDecoderState_t NeroIntelCE4x00DecoderGetState();

/* private variables */
NeroDecoderState_t NeroDemuxState;
ismd_dev_t demux_stream_handle;
ismd_port_handle_t demux_input_port_handle;
ismd_port_handle_t ts_vid_output_port_handle;
ismd_port_handle_t ts_aud_output_port_handle;

ismd_demux_filter_handle_t demux_ves_filter_handle;
ismd_demux_filter_handle_t demux_aes_filter_handle;
ismd_demux_filter_handle_t demux_wts_filter_handle;
ismd_port_handle_t dummy_port_handle;

nero_clock_handle_t clock_handle;
};
#endif /*NERODEMUX_H_*/
