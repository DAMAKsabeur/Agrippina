#include "NeroDemux.h"


/* private functions */

ismd_result_t NeroDemux::NeroIntelCE4x00DemuxInvalidateHandles()
{
   ismd_result_t ismd_ret  = ISMD_SUCCESS;
   demux_stream_handle = ISMD_DEV_HANDLE_INVALID;
   
   demux_input_port_handle = ISMD_PORT_HANDLE_INVALID;
   ts_vid_output_port_handle = ISMD_PORT_HANDLE_INVALID;
   ts_aud_output_port_handle = ISMD_PORT_HANDLE_INVALID;
   dummy_port_handle  = ISMD_PORT_HANDLE_INVALID;
   demux_ves_filter_handle = ISMD_DEV_HANDLE_INVALID;
   demux_aes_filter_handle = ISMD_DEV_HANDLE_INVALID;
   demux_wts_filter_handle = ISMD_DEV_HANDLE_INVALID;
   return (ismd_ret);
}
ismd_result_t NeroDemux::NeroIntelCE4x00DemuxSend_new_segment()
{
   ismd_newsegment_tag_t newsegment_data;
   ismd_buffer_handle_t carrier_buffer;
   ismd_result_t ismd_ret;

   newsegment_data.linear_start = 0;
   newsegment_data.start = ISMD_NO_PTS;
   newsegment_data.stop = ISMD_NO_PTS;
   newsegment_data.requested_rate = 10000;
   newsegment_data.applied_rate = ISMD_NORMAL_PLAY_RATE;
   newsegment_data.rate_valid = true;

   _TRY(ismd_ret,ismd_buffer_alloc,(0, &carrier_buffer));
   _TRY(ismd_ret,ismd_tag_set_newsegment,(carrier_buffer, newsegment_data));
   _TRY(ismd_ret,ismd_port_write,(demux_input_port_handle, carrier_buffer));
error:
     return ismd_ret;	
	
}

ismd_result_t NeroDemux::NeroIntelCE4x00DemuxInit(uint16_t audio_pid, uint16_t video_pid, uint16_t pcr_pid,ismd_port_handle_t audio_input_port,ismd_port_handle_t video_input_port)
{
		/* demux */
    ismd_result_t ismd_ret;
        int time_valid = 0x01;

    ismd_time_t curr_time = 0x00;
   /*_TRY(ismd_ret, build_demux,(audio_pid, video_pid, pcr_pid));*/
    unsigned output_buf_size = 0;
    ismd_demux_pid_list_t demux_pid_list;

    _TRY(ismd_ret,ismd_demux_stream_open,(ISMD_DEMUX_STREAM_TYPE_MPEG2_TRANSPORT_STREAM, &demux_stream_handle, &demux_input_port_handle));
    output_buf_size = 32*1024;
    _TRY(ismd_ret,ismd_demux_filter_open,(demux_stream_handle, ISMD_DEMUX_OUTPUT_ES, output_buf_size, &demux_ves_filter_handle, &dummy_port_handle));
    _TRY(ismd_ret,ismd_demux_filter_get_output_port,(demux_stream_handle, demux_ves_filter_handle, &ts_vid_output_port_handle));
    output_buf_size = 8*1024;
    _TRY(ismd_ret,ismd_demux_filter_open,(demux_stream_handle, ISMD_DEMUX_OUTPUT_ES, output_buf_size, &demux_aes_filter_handle, &dummy_port_handle));
    _TRY(ismd_ret,ismd_demux_filter_get_output_port,(demux_stream_handle, demux_aes_filter_handle, &ts_aud_output_port_handle));
    demux_pid_list.pid_array[0].pid_type = ISMD_DEMUX_PID_TYPE_PES;
    demux_pid_list.pid_array[0].pid_val = video_pid;
    _TRY(ismd_ret,ismd_demux_filter_map_to_pids,(demux_stream_handle, demux_ves_filter_handle, demux_pid_list, 1));
    demux_pid_list.pid_array[0].pid_type = ISMD_DEMUX_PID_TYPE_PES;
    demux_pid_list.pid_array[0].pid_val = audio_pid;
    _TRY(ismd_ret,ismd_demux_filter_map_to_pids,(demux_stream_handle, demux_aes_filter_handle, demux_pid_list, 1));
    _TRY(ismd_ret,ismd_demux_ts_set_pcr_pid,(demux_stream_handle, pcr_pid));
    _TRY(ismd_ret,ismd_dev_set_state,(demux_stream_handle,ISMD_DEV_STATE_PAUSE));
    _TRY(ismd_ret,ismd_dev_set_clock,(demux_stream_handle, clock_handle));
    _TRY(ismd_ret,ismd_demux_stream_use_arrival_time,(demux_stream_handle, false));



  /* _TRY(ismd_ret, assign_demux_clock,());*/
    _TRY(ismd_ret,ismd_dev_set_clock,(demux_stream_handle, clock_handle));
    _TRY(ismd_ret,ismd_dev_set_clock,(demux_stream_handle, clock_handle));
    _TRY(ismd_ret,ismd_demux_stream_use_arrival_time,(demux_stream_handle, false));



   /*_TRY(ismd_ret, connect_demux_pipeline,());*/

    _TRY(ismd_ret,ismd_port_connect,(ts_aud_output_port_handle, audio_input_port));
    _TRY(ismd_ret,ismd_port_connect,(ts_vid_output_port_handle, video_input_port));

   /*_TRY(ismd_ret, configure_demux_clocks,(0, 1));*/
    if(time_valid)
         _TRY(ismd_ret,ismd_clock_set_time,(clock_handle, curr_time));
    else
         _TRY(ismd_ret,ismd_clock_get_time,(clock_handle, &curr_time));

    _TRY(ismd_ret,ismd_dev_set_stream_base_time,(demux_stream_handle, curr_time));

   /*_TRY(ismd_ret, start_demux_pipeline,());*/
    _TRY(ismd_ret,NeroIntelCE4x00DemuxPlay,());

   _TRY(ismd_ret, NeroIntelCE4x00DemuxSend_new_segment,());
error:
    return ismd_ret;
}
ismd_result_t NeroDemux::NeroIntelCE4x00DemuxUnInit()
{
    ismd_result_t ismd_ret;
   /*_TRY(ismd_ret,pause_demux_pipeline,());*/
   _TRY(ismd_ret,NeroIntelCE4x00DemuxPause,());

   /*_TRY(ismd_ret,stop_demux_pipeline,());*/
   _TRY(ismd_ret,NeroIntelCE4x00DemuxStop,());
   /*_TRY(ismd_ret,flush_demux_pipeline,());*/
   _TRY(ismd_ret,NeroIntelCE4x00DemuxFlush,());
   /*_TRY(ismd_ret,disconnect_demux_pipeline,());*/
   _TRY(ismd_ret, ismd_port_disconnect,(ts_aud_output_port_handle));
   _TRY(ismd_ret, ismd_port_disconnect,(ts_vid_output_port_handle));
   /*_TRY(ismd_ret,close_demux_pipeline,());	*/
   _TRY(ismd_ret,NeroIntelCE4x00DemuxClose,());
   
error:

   return (ismd_ret);
}
ismd_result_t NeroDemux::NeroIntelCE4x00DemuxReconfigure (uint16_t audio_pid, uint16_t video_pid, uint16_t pcr_pid)
{
    ismd_result_t ismd_ret = ISMD_SUCCESS;

	return ismd_ret;
}


ismd_result_t NeroDemux::NeroIntelCE4x00DemuxStop()
{
   ismd_result_t ismd_ret;
   _TRY(ismd_ret,ismd_dev_set_state,(demux_stream_handle, ISMD_DEV_STATE_STOP));

error:
    return ismd_ret;
}

ismd_result_t NeroDemux::NeroIntelCE4x00DemuxPause()
{
   ismd_result_t ismd_ret;
   _TRY(ismd_ret,ismd_dev_set_state,(demux_stream_handle, ISMD_DEV_STATE_PAUSE));

error:
    return ismd_ret;
	
	
}
ismd_result_t NeroDemux::NeroIntelCE4x00DemuxPlay()
{
    ismd_result_t ismd_ret;
    _TRY(ismd_ret,ismd_dev_set_state,(demux_stream_handle, ISMD_DEV_STATE_PLAY));
error:
    return ismd_ret;	
}

ismd_result_t NeroDemux::NeroIntelCE4x00DemuxFlush()
{
    ismd_result_t ismd_ret;
    _TRY(ismd_ret,ismd_dev_flush,(demux_stream_handle));
error:
    return ismd_ret;
}
ismd_result_t NeroDemux::NeroIntelCE4x00DemuxClose()
{
   ismd_result_t ismd_ret;

   _TRY(ismd_ret, ismd_dev_close,(demux_stream_handle));

error:
    return ismd_ret;
}

ismd_result_t NeroDemux::NeroIntelCE4x00DemuxFeed(char* buff , size_t size)
{

   ismd_buffer_handle_t ismd_buffer_handle;
   ismd_result_t ismd_ret;
   ismd_buffer_descriptor_t ismd_buf_desc;
   uint8_t *buf_ptr;
   
   
 while (ismd_buffer_alloc(size, &ismd_buffer_handle) != ISMD_SUCCESS) {
         //OS_INFO("trying to get buffer\n");
         os_sleep(20);
      }

      _TRY(ismd_ret, ismd_buffer_read_desc,(ismd_buffer_handle, &ismd_buf_desc));

      buf_ptr = (uint8_t *)OS_MAP_IO_TO_MEM_NOCACHE(ismd_buf_desc.phys.base,ismd_buf_desc.phys.size);

      memcpy(buf_ptr, buff, size);

      ismd_buf_desc.phys.level = size;

      _TRY(ismd_ret, ismd_buffer_update_desc,(ismd_buffer_handle, &ismd_buf_desc));

      OS_UNMAP_IO_FROM_MEM(buf_ptr,size);

      while (ismd_port_write(demux_input_port_handle, ismd_buffer_handle) != ISMD_SUCCESS) {
         os_sleep(20);
      }

   //OS_INFO("Done writing file in\n");

error:
    return ismd_ret;
}


/* public functions */
NeroDemux::NeroDemux()
{
   ismd_result_t ismd_ret  = ISMD_SUCCESS;

	ismd_ret = NeroIntelCE4x00DemuxInvalidateHandles();
}

NeroDemux::NeroDemux(nero_clock_handle_t clk)
{
   ismd_result_t ismd_ret  = ISMD_SUCCESS;
   ismd_ret = NeroIntelCE4x00DemuxInvalidateHandles();
   nero_clock_handle_t clock_handle = clk;
}

NeroDemux::~NeroDemux()
{
   ismd_result_t ismd_ret  = ISMD_SUCCESS;
   ismd_ret = NeroIntelCE4x00DemuxInvalidateHandles();
}

Nero_error_t NeroDemux::NeroDemuxInit (uint16_t audio_pid, uint16_t video_pid, uint16_t pcr_pid,ismd_port_handle_t audio_input_port,ismd_port_handle_t video_input_port)
{
	Nero_error_t result = NERO_SUCCESS;
	ismd_result_t ismd_ret;
    _TRY(ismd_ret,NeroIntelCE4x00DemuxInit,(audio_pid , video_pid , pcr_pid , audio_input_port, video_input_port));
error:
     if (ISMD_SUCCESS != ismd_ret)
     {
         result = NERO_ERROR_INTERNAL;
         NERO_ERROR("NeroIntelCE4x00DemuxInit fail (%d)",ismd_ret);
	 }
     return (result);
}
Nero_error_t NeroDemux::NeroDemuxReconfigure(uint16_t audio_pid, uint16_t video_pid, uint16_t pcr_pid)
{
	Nero_error_t result = NERO_SUCCESS;
	ismd_result_t ismd_ret;
    _TRY(ismd_ret,NeroIntelCE4x00DemuxReconfigure,(audio_pid , video_pid , pcr_pid));
error:
     if (ISMD_SUCCESS != ismd_ret)
     {
         result = NERO_ERROR_INTERNAL;
         NERO_ERROR("NeroIntelCE4x00DemuxInit fail (%d)",ismd_ret);
	 }
     return (result);
}
Nero_error_t NeroDemux::NeroDemuxUnInit()
{
	Nero_error_t result = NERO_SUCCESS;
	ismd_result_t ismd_ret;
    _TRY(ismd_ret,NeroIntelCE4x00DemuxUnInit,());
error:
     if (ISMD_SUCCESS != ismd_ret)
     {
         result = NERO_ERROR_INTERNAL;
         NERO_ERROR("NeroIntelCE4x00DemuxUnInit fail (%d)",ismd_ret);
	 }
     return (result);
}
Nero_error_t NeroDemux::NeroDemuxFlush()
{
	Nero_error_t result = NERO_SUCCESS;
	ismd_result_t ismd_ret;
    _TRY(ismd_ret,NeroIntelCE4x00DemuxFlush,());
error:
     if (ISMD_SUCCESS != ismd_ret)
     {
         result = NERO_ERROR_INTERNAL;
         NERO_ERROR("NeroIntelCE4x00DemuxFlush fail (%d)",ismd_ret);
	 }
     return (result);
}
Nero_error_t NeroDemux::NeroDemuxStop()
{
	Nero_error_t result = NERO_SUCCESS;
	ismd_result_t ismd_ret;
    _TRY(ismd_ret,NeroIntelCE4x00DemuxStop,());
error:
     if (ISMD_SUCCESS != ismd_ret)
     {
         result = NERO_ERROR_INTERNAL;
         NERO_ERROR("NeroIntelCE4x00DemuxStop fail (%d)",ismd_ret);
	 }
     return (result);
}
Nero_error_t NeroDemux::NeroDemuxPause()
{
	Nero_error_t result = NERO_SUCCESS;
	ismd_result_t ismd_ret;
    _TRY(ismd_ret,NeroIntelCE4x00DemuxPause,());
error:
     if (ISMD_SUCCESS != ismd_ret)
     {
         result = NERO_ERROR_INTERNAL;
         NERO_ERROR("NeroIntelCE4x00DemuxPause fail (%d)",ismd_ret);
	 }
     return (result);
}
Nero_error_t NeroDemux::NeroDemuxPlay()
{
	Nero_error_t result = NERO_SUCCESS;
	ismd_result_t ismd_ret;
    _TRY(ismd_ret,NeroIntelCE4x00DemuxPlay,());
error:
     if (ISMD_SUCCESS != ismd_ret)
     {
         result = NERO_ERROR_INTERNAL;
         NERO_ERROR("NeroIntelCE4x00DemuxPlay fail (%d)",ismd_ret);
	 }
     return (result);
}
/*NeroDecoderState_t NeroDemuxGetState()
{

}
Nero_error_t NeroDemuxSetState(NeroDecoderState_t NeroDemuxSetState)
{
}
nero_time_t NeroDemuxGetPauseTime()
{
	
}*/
nero_time_t NeroDemux::NeroDemuxFeed(char* buff , size_t size)
{
	Nero_error_t result = NERO_SUCCESS;
	ismd_result_t ismd_ret;
    _TRY(ismd_ret,NeroIntelCE4x00DemuxFeed,(buff, size));
error:
     if (ISMD_SUCCESS != ismd_ret)
     {
         result = NERO_ERROR_INTERNAL;
         NERO_ERROR("NeroIntelCE4x00DemuxPlay fail (%d)",ismd_ret);
	 }
     return (result);
}
