#include "NeroIntelCE4x00VideoDecoder.h"

using namespace std;

/*************************************************************************
 * private:
**************************************************************************/

extern "C"
{
void *vid_evt_handler(void* args)
{
	NeroIntelCE4x00VideoDecoder*  m_NeroIntelCE4x00VideoDecoder = (NeroIntelCE4x00VideoDecoder*)args;
	   ismd_result_t ismd_ret=ISMD_SUCCESS;
	   ismd_event_t triggered_event=ISMD_EVENT_HANDLE_INVALID;

	   while(! m_NeroIntelCE4x00VideoDecoder->vid_evt_handler_thread_exit) {
	      ismd_ret=ismd_event_wait_multiple(m_NeroIntelCE4x00VideoDecoder->ismd_vid_evt_tab, m_NeroIntelCE4x00VideoDecoder->nb_vid_evt, EVENT_TIMEOUT, &triggered_event);
	      if(ismd_ret!=ISMD_ERROR_TIMEOUT) {
	         if (ismd_ret!=ISMD_SUCCESS) {
	            printf("vid event waited failed (%d)\n", ismd_ret);
	         }
	         else {
	            //printf("\t--> VID EVT RECEIVED : %d (%s)\n", triggered_event, dbg_evt(triggered_event, VIDEO_EVT));
	            //if(triggered_event == ISMD_VIDREND_EVENT_TYPE_EOS) {
	            //   video_eos_reached = true;
	           // }
	            ismd_ret=ismd_event_acknowledge(triggered_event);
	            if (ismd_ret!=ISMD_SUCCESS){
	               printf("ismd_event_acknowledge failed (%d)\n", ismd_ret);
	               OS_ASSERT(0);
	            }
	         }
	      }
	      if(triggered_event !=ISMD_EVENT_HANDLE_INVALID)
	      {
	    	pthread_mutex_lock(&m_NeroIntelCE4x00VideoDecoder->mutex_stock);
	    	m_NeroIntelCE4x00VideoDecoder->triggered_event = triggered_event;
	    	pthread_mutex_unlock(&m_NeroIntelCE4x00VideoDecoder->mutex_stock);
	        m_NeroIntelCE4x00VideoDecoder->Notify();


	      }
	      usleep(10000);
	   }
	   printf("exit vid_evt_handler \n");
	   return NULL;
}
}

Nero_error_t NeroIntelCE4x00VideoDecoder::NeroIntelCE4x00VideoDecoderInvalidateHandles()
{

    NERO_VIDEO_NOTICE ("NeroIntelCE4x00VideoDecoderInvalidateHandles ... \n");
    Nero_error_t     result = NERO_SUCCESS;
    ismd_result_t ismd_ret = ISMD_SUCCESS;
    clock_handle    = ISMD_DEV_HANDLE_INVALID;
    viddec_handle   = ISMD_DEV_HANDLE_INVALID;
    vidpproc_handle = ISMD_DEV_HANDLE_INVALID;
    vidrend_handle  = ISMD_DEV_HANDLE_INVALID;
    vidsink_handle  = ISMD_DEV_HANDLE_INVALID;
    viddec_input_port_handle   = ISMD_PORT_HANDLE_INVALID;
    viddec_output_port_handle  = ISMD_PORT_HANDLE_INVALID;
    vidpproc_input_port_handle = ISMD_PORT_HANDLE_INVALID;

    return(result);

}

ismd_codec_type_t NeroIntelCE4x00VideoDecoder::NeroIntelCE4x00VideoDecoder_NERO2ISMD_codeRemap(Nero_video_codec_t NeroVideoAlgo)
{

    NERO_VIDEO_NOTICE ("NeroIntelCE4x00VideoDecoder_NERO2ISMD_codeRemap ...%d \n",NeroVideoAlgo);
    ismd_codec_type_t vid_fmt = ISMD_CODEC_TYPE_H264;
    switch (NeroVideoAlgo)
    {

    case NERO_CODEC_VIDEO_MPEG2   : /**< MPEG2 video codec type. */
    {
    	vid_fmt = ISMD_CODEC_TYPE_MPEG2 ;
    	break;
    }
    case NERO_CODEC_VIDEO_H264    : /**< H264 video codec type. */
    {
    	vid_fmt = ISMD_CODEC_TYPE_H264;
    	break;
    }
    case NERO_CODEC_VIDEO_VC1     : /**< VC1 video codec type. */
    {
    	vid_fmt = ISMD_CODEC_TYPE_VC1 ;
    	break;
    }
    case NERO_CODEC_VIDEO_MPEG4   : /**< MPEG4 video codec type. */
    {
    	vid_fmt = ISMD_CODEC_TYPE_MPEG4;
    	break;
    }
    case NERO_CODEC_VIDEO_JPEG    : /**< JPEG video codec type. */
    {
    	vid_fmt = ISMD_CODEC_TYPE_JPEG;
    	break;
    }
    case NERO_CODEC_VIDEO_INVALID : /**< A known invalid codec type. */
    default:
    {
    	vid_fmt = ISMD_CODEC_TYPE_INVALID;
    	break;
    }
    }
    NERO_VIDEO_NOTICE ("ismd_codec_type_t ...%d \n",vid_fmt);
    return ((ismd_codec_type_t) vid_fmt);

}

Nero_error_t NeroIntelCE4x00VideoDecoder::NeroIntelCE4x00VideoDecoderSend_new_segment()
{

    NERO_VIDEO_NOTICE ("NeroIntelCE4x00VideoDecoderSend_new_segment ... \n");
    Nero_error_t     result = NERO_SUCCESS;

    ismd_newsegment_tag_t newsegment_data;
    ismd_buffer_handle_t carrier_buffer;
    ismd_result_t ismd_ret;

    newsegment_data.linear_start = 0;
    newsegment_data.start = ISMD_NO_PTS;
    newsegment_data.stop = ISMD_NO_PTS;
    newsegment_data.requested_rate = 10000;
    newsegment_data.applied_rate = ISMD_NORMAL_PLAY_RATE;
    newsegment_data.rate_valid = true;

    _TRY(ismd_ret,result,ismd_buffer_alloc,(0, &carrier_buffer));
    _TRY(ismd_ret,result,ismd_tag_set_newsegment,(carrier_buffer, newsegment_data));
    _TRY(ismd_ret,result,ismd_port_write,(viddec_input_port_handle, carrier_buffer));
error:
     return (result);
}

ismd_result_t NeroIntelCE4x00VideoDecoder::NeroIntelCE4x00VideoDecoder_subscribe_events()
{
   ismd_result_t ismd_ret;
   int i=0;
   NERO_AUDIO_NOTICE ("NeroIntelCE4x00VideoDecoder_subscribe_events ...  \n");
   // subscribe video events
   if(VIDEO_NB_EVENT_TO_MONITOR>0) {

      // viddec events
      for(i=0;i<VIDDEC_NB_EVENT_TO_MONITOR;i++) {
         ismd_ret = ismd_viddec_get_event(viddec_handle, viddec_evt_to_monitor[i], &ismd_vid_evt_tab[nb_vid_evt++]);
         if(ismd_ret != ISMD_SUCCESS) {
           printf("failed at ismd_viddec_get_event on %d (ismd_evt = %d) : ismd_ret = %d\n",i, viddec_evt_to_monitor[i], ismd_ret);
         }
      }

      // vidrend events
      for(i=0;i<VIDREND_NB_EVENT_TO_MONITOR;i++) {
         switch(vidrend_evt_to_monitor[i]) {

            case ISMD_VIDREND_EVENT_TYPE_FRAME_FLIPPED: //-----------------------------------SYNC_IN
               ismd_ret = ismd_vidrend_get_frame_flipped_event(vidrend_handle, &ismd_vid_evt_tab[nb_vid_evt++]);
               if(ismd_ret != ISMD_SUCCESS) {
                  printf("failed at ismd_vidrend_get_frame_flipped_event\n");
               }
               break;

            case ISMD_VIDREND_EVENT_TYPE_UNDERRUN: //---------------------------------------SYNC_OUT
               ismd_ret = ismd_event_alloc(&ismd_vid_evt_tab[nb_vid_evt]);
               if(ismd_ret != ISMD_SUCCESS) {
                  printf("failed at ismd_event_alloc\n");
               }
               ismd_ret = ismd_dev_set_underrun_event(vidrend_handle, ismd_vid_evt_tab[nb_vid_evt]);
               if(ismd_ret != ISMD_SUCCESS) {
                  printf("failed at ismd_vidrend_set_underrun_event_event\n");
               }
               nb_vid_evt++;
               break;

            case ISMD_VIDREND_EVENT_TYPE_UNDERFLOW:
               ismd_ret = ismd_vidrend_get_underflow_event(vidrend_handle, &ismd_vid_evt_tab[nb_vid_evt++]);
               if(ismd_ret != ISMD_SUCCESS) {
                  printf("failed at ismd_vidrend_get_underflow_event\n");
               }
               break;

            case ISMD_VIDREND_EVENT_TYPE_UNDERFLOW_RECOVERED :
               ismd_ret = ismd_vidrend_get_underflow_recovered_event(vidrend_handle, &ismd_vid_evt_tab[nb_vid_evt++]);
               if(ismd_ret != ISMD_SUCCESS) {
                  printf("failed at ismd_vidrend_get_underflow_recovered_event  \n");
               }
               break;
               default : printf("The vidrend event %d can not be subscribed yet \n", vidrend_evt_to_monitor[i]);
         }
      }
   }
   return (ismd_ret);
}


ismd_result_t NeroIntelCE4x00VideoDecoder::NeroIntelCE4x00VideoDecoder_unsubscribe_events()
{
  ismd_result_t ismd_ret;
  int i=0;

  // video events
  if(VIDEO_NB_EVENT_TO_MONITOR>0) {
     for(i=0;i<VIDEO_NB_EVENT_TO_MONITOR;i++) {
        ismd_vid_evt_tab[i]=ISMD_EVENT_HANDLE_INVALID;
        ismd_ret = ismd_event_free(ismd_vid_evt_tab[i]);
        if(ismd_ret != ISMD_SUCCESS) {
           printf("failed at ismd_event_free on %d (ismd_evt = %d)\n",i, ismd_vid_evt_tab[i]);
        }
     }
  }
  return (ismd_ret);
}

/*************************************************************************
 * public:
**************************************************************************/

NeroIntelCE4x00VideoDecoder::NeroIntelCE4x00VideoDecoder()
{

    NERO_VIDEO_NOTICE ("NeroIntelCE4x00VideoDecoder creator  ... \n");
    Nero_error_t     result = NERO_SUCCESS;
    ismd_result_t ismd_ret;
    internal_clk = true;
    nb_vid_evt=0;
    result = NeroIntelCE4x00VideoDecoderInvalidateHandles();
    ismd_ret = ismd_clock_alloc(ISMD_CLOCK_TYPE_FIXED, &clock_handle);
    if (ISMD_SUCCESS != ismd_ret)
    {
	    result = NERO_ERROR_INTERNAL;
	}
    viddec_evt_to_monitor = {
          ISMD_VIDDEC_OUT_OF_MEMORY,        //Buffer allocation failures.
          //ISMD_VIDDEC_RESOLUTION_CHANGE,    //Resolution, aspect ratio, or frame rate change.
          //ISMD_VIDDEC_EOS,                  //End of stream detected.
          //ISMD_VIDDEC_BITSTREAM_ERROR,      //Bitstream error detected.
          ISMD_VIDDEC_DROPPED_FRAMES,       //Error frames dropped.
          //ISMD_VIDDEC_CLIENT_ID_SEEN,       //Client id seen.
          ISMD_VIDDEC_UNDERFLOW,            //ISMD_VIDDEC_UNDERFLOW and ISMD_VIDDEC_UNDERFLOW_RECOVERED event are
                                            //supported in CE4100 and next generations, but not CE3100. Input underflow event.
          ISMD_VIDDEC_UNDERFLOW_RECOVERED,  //Back to normal after underflow.
          //ISMD_VIDDEC_STREAM_FORMAT_CHANGE  //Change in Stream format detected.
       };
    vidrend_evt_to_monitor = {
          //ISMD_VIDREND_EVENT_TYPE_ERROR,                //An error occured in vidrend - skipped vsync, late flip, late frames
                                                        //or out of order frames.
          ISMD_VIDREND_EVENT_TYPE_VSYNC_FRAME,          //Interrupt received from display with frame polarity vsyncs_frame_received in ismd_vidrend_stats_t will be updated.
          ISMD_VIDREND_EVENT_TYPE_VSYNC_FIELD_TOP,      //Interrupt received from display with top field polarity.
          ISMD_VIDREND_EVENT_TYPE_VSYNC_FIELD_BOTTOM,   //Interrupt recevied from display with bottom field polairty.
          //ISMD_VIDREND_EVENT_TYPE_RES_CHG,              //Resolution change from next frame.
          //ISMD_VIDREND_EVENT_TYPE_EOS,                  //End of stream.
          //ISMD_VIDREND_EVENT_TYPE_CLIENT_ID_SEEN,       //Client ID Last seen by the renderer.
          //ISMD_VIDREND_EVENT_TYPE_EOSEG,                //End of segment.
          ISMD_VIDREND_EVENT_TYPE_UNDERRUN,             //Underrun Event.
          //ISMD_VIDREND_EVENT_TYPE_SOSEG,                //Start of segment.
          ISMD_VIDREND_EVENT_TYPE_FRAME_FLIPPED,        //Frame flipped event.
          ISMD_VIDREND_EVENT_TYPE_UNDERFLOW,            //Input Underflow event This event was triggered when input of vidrend was empty.
          ISMD_VIDREND_EVENT_TYPE_UNDERFLOW_RECOVERED,  //Input Underflow recovered event This event was triggered when underflow state recovered.
       };
    ismd_vid_evt_tab = {0,};
    vid_evt_handler_thread_exit = false;
    if (pthread_mutex_init(&mutex_stock, NULL) != 0)
    {
        result = NERO_ERROR_INTERNAL;
        NERO_AUDIO_ERROR("mutex_stock  init failed \n");
    }
}
/** Nero ISMD Video decoder constructor */
NeroIntelCE4x00VideoDecoder::NeroIntelCE4x00VideoDecoder(NeroSTC* NeroSTC_ptr)
{
	nb_vid_evt=0;
    NERO_VIDEO_NOTICE ("NeroIntelCE4x00VideoDecoder creator  ... \n");
    Nero_error_t     result = NERO_SUCCESS;
    result = NeroIntelCE4x00VideoDecoderInvalidateHandles();
    internal_clk = false;
    clock_handle = (ismd_clock_t)NeroSTC_ptr->NeroSTCGetClock();
    stc = NeroSTC_ptr;
    viddec_evt_to_monitor = {
          ISMD_VIDDEC_OUT_OF_MEMORY,        //Buffer allocation failures.
          //ISMD_VIDDEC_RESOLUTION_CHANGE,    //Resolution, aspect ratio, or frame rate change.
          //ISMD_VIDDEC_EOS,                  //End of stream detected.
          //ISMD_VIDDEC_BITSTREAM_ERROR,      //Bitstream error detected.
          ISMD_VIDDEC_DROPPED_FRAMES,       //Error frames dropped.
          //ISMD_VIDDEC_CLIENT_ID_SEEN,       //Client id seen.
          ISMD_VIDDEC_UNDERFLOW,            //ISMD_VIDDEC_UNDERFLOW and ISMD_VIDDEC_UNDERFLOW_RECOVERED event are
                                            //supported in CE4100 and next generations, but not CE3100. Input underflow event.
          ISMD_VIDDEC_UNDERFLOW_RECOVERED,  //Back to normal after underflow.
          //ISMD_VIDDEC_STREAM_FORMAT_CHANGE  //Change in Stream format detected.
       };
    vidrend_evt_to_monitor = {
          //ISMD_VIDREND_EVENT_TYPE_ERROR,                //An error occured in vidrend - skipped vsync, late flip, late frames
                                                        //or out of order frames.
          ISMD_VIDREND_EVENT_TYPE_VSYNC_FRAME,          //Interrupt received from display with frame polarity vsyncs_frame_received in ismd_vidrend_stats_t will be updated.
          ISMD_VIDREND_EVENT_TYPE_VSYNC_FIELD_TOP,      //Interrupt received from display with top field polarity.
          ISMD_VIDREND_EVENT_TYPE_VSYNC_FIELD_BOTTOM,   //Interrupt recevied from display with bottom field polairty.
          //ISMD_VIDREND_EVENT_TYPE_RES_CHG,              //Resolution change from next frame.
          //ISMD_VIDREND_EVENT_TYPE_EOS,                  //End of stream.
          //ISMD_VIDREND_EVENT_TYPE_CLIENT_ID_SEEN,       //Client ID Last seen by the renderer.
          //ISMD_VIDREND_EVENT_TYPE_EOSEG,                //End of segment.
          ISMD_VIDREND_EVENT_TYPE_UNDERRUN,             //Underrun Event.
          //ISMD_VIDREND_EVENT_TYPE_SOSEG,                //Start of segment.
          ISMD_VIDREND_EVENT_TYPE_FRAME_FLIPPED,        //Frame flipped event.
          ISMD_VIDREND_EVENT_TYPE_UNDERFLOW,            //Input Underflow event This event was triggered when input of vidrend was empty.
          ISMD_VIDREND_EVENT_TYPE_UNDERFLOW_RECOVERED,  //Input Underflow recovered event This event was triggered when underflow state recovered.
       };
    ismd_vid_evt_tab = {0,};
    vid_evt_handler_thread_exit = false;
    if (pthread_mutex_init(&mutex_stock, NULL) != 0)
    {
        result = NERO_ERROR_INTERNAL;
        NERO_AUDIO_ERROR("mutex_stock  init failed \n");
    }
}

/** Nero ISMD Video decoder constructor */
NeroIntelCE4x00VideoDecoder::~NeroIntelCE4x00VideoDecoder()
{

    NERO_VIDEO_NOTICE ("NeroIntelCE4x00VideoDecoder killer ... \n");
    Nero_error_t     result = NERO_SUCCESS;
    ismd_result_t ismd_ret;
    if(pthread_mutex_destroy(&mutex_stock)!=0)
    {
        result = NERO_ERROR_INTERNAL;
        NERO_AUDIO_ERROR("mutex_stock mutex destruction  fail ... (%d) \n", result);
    }
    if(internal_clk == true)
    {
	    _TRY(ismd_ret,result, ismd_clock_free,(clock_handle));
	}
    stc = NULL;
error:
    result = NeroIntelCE4x00VideoDecoderInvalidateHandles();
}

/** Nero ISMD Video decoder to init and start the decoder  */
Nero_error_t NeroIntelCE4x00VideoDecoder::NeroVideoDecoderInit (Nero_video_codec_t NeroVideoAlgo)
{
    /* Video */
    NERO_VIDEO_NOTICE ("NeroVideoDecoderInit  ... \n");
    Nero_error_t result = NERO_SUCCESS;
    ismd_result_t ismd_ret;
    ismd_vidsink_scale_params_t sp;
    int time_valid = 0x01;
    ismd_time_t curr_time = 0x00;
    orginal_clock_time = 0x00;
    Nero_stc_type_t stc_type = NERO_STC_FREERUN;
    ismd_codec_type_t vid_fmt = NeroIntelCE4x00VideoDecoder_NERO2ISMD_codeRemap(NeroVideoAlgo);
    /*_TRY(ismd_ret,result, build_viddec,(video_algo));*/
    _TRY(ismd_ret,result,ismd_viddec_open,(vid_fmt, &viddec_handle));
    _TRY(ismd_ret,result,ismd_viddec_set_pts_interpolation_policy,(viddec_handle, ISMD_VIDDEC_INTERPOLATE_MISSING_PTS , ISMD_NO_PTS));
    _TRY(ismd_ret,result,ismd_viddec_set_frame_mask,(viddec_handle, ISMD_VIDDEC_SKIP_NONE));
    _TRY(ismd_ret,result,ismd_viddec_set_max_frames_to_decode,(viddec_handle,ISMD_VIDDEC_ALL_FRAMES));
    _TRY(ismd_ret,result,ismd_viddec_set_frame_error_policy,(viddec_handle, ISMD_VIDDEC_EMIT_ALL));
    _TRY(ismd_ret,result,ismd_viddec_get_input_port,(viddec_handle, &viddec_input_port_handle));
    _TRY(ismd_ret,result,ismd_viddec_get_output_port,(viddec_handle, &viddec_output_port_handle));
    _TRY(ismd_ret,result,ismd_dev_set_state,(viddec_handle,ISMD_DEV_STATE_PAUSE));
    /*_TRY(ismd_ret,result, build_vidpproc,());*/
    _TRY(ismd_ret,result,ismd_vidpproc_open,(&vidpproc_handle));
    /*_TRY(ismd_ret,result, build_vidrend,());*/
    _TRY(ismd_ret,result,ismd_vidrend_open,(&vidrend_handle));
    _TRY(ismd_ret,result,ismd_vidrend_set_video_plane,(vidrend_handle, layer));
    if (ISMD_CODEC_TYPE_H264 == vid_fmt) {
        _TRY(ismd_ret,result,ismd_vidrend_enable_max_hold_time,(vidrend_handle,10,1));
    }
    /*_TRY(ismd_ret,result,build_vidsink,());*/


    _TRY(ismd_ret,result,ismd_vidsink_open,(&vidsink_handle));
    _TRY(ismd_ret,result,ismd_vidsink_set_smd_handles,(vidsink_handle,vidpproc_handle, vidrend_handle));
    _TRY(ismd_ret,result,ismd_vidsink_get_input_port,(vidsink_handle,&vidpproc_input_port_handle));
    _TRY(ismd_ret,result,ismd_vidsink_enable_frame_by_frame_mode,(vidsink_handle, 0));
    sp.crop_window.h_offset = 0;
    sp.crop_window.v_offset = 0;
    sp.crop_window.width = DISPLAY_WIDTH;
    sp.crop_window.height = DISPLAY_HEIGHT;
    sp.crop_enable = 0;
    sp.dest_window.x=0;
    sp.dest_window.y=0;
    sp.dest_window.width = DISPLAY_WIDTH;
    sp.dest_window.height = DISPLAY_HEIGHT;
    sp.aspect_ratio.numerator = 1;
    sp.aspect_ratio.denominator = 1;
    sp.scaling_policy = SCALE_TO_FIT;
    _TRY(ismd_ret,result,ismd_vidsink_set_global_scaling_params,(vidsink_handle,sp));
    _TRY(ismd_ret,result,ismd_vidsink_set_flush_policy,(vidsink_handle,ISMD_VIDSINK_FLUSH_POLICY_REPEAT_FRAME  ));
    _TRY(ismd_ret,result,ismd_vidsink_set_pause_policy,(vidsink_handle, ISMD_VIDSINK_PAUSE_POLICY_ANIMATION));
    _TRY(ismd_ret,result,ismd_vidsink_set_state,(vidsink_handle,ISMD_DEV_STATE_PAUSE));
    /*_TRY(ismd_ret,result, assign_video_clock,());*/
    _TRY(ismd_ret,result,ismd_vidsink_set_clock,(vidsink_handle, clock_handle));
    /*_TRY(ismd_ret,result, connect_video_pipeline,());*/
    _TRY(ismd_ret,result,ismd_port_connect,(viddec_output_port_handle, vidpproc_input_port_handle));
    //_TRY(ismd_ret,result,ismd_clock_set_time,(clock_handle, curr_time));
    /*_TRY(ismd_ret,result, configure_video_clocks,(0, 1));*/
	//_TRY(ismd_ret,result,ismd_clock_get_time,(clock_handle, &curr_time));
    stc_type = stc->NeroSTCGetType();
    switch(stc_type)
    {
        case NERO_STC_AUDIO_MASTER:
        {
        	//_TRY(ismd_ret,result,ismd_clock_get_time,(clock_handle, &curr_time));
        	curr_time = stc->NeroSTCGetBaseTime();
        	_TRY(ismd_ret,result,ismd_clock_set_time,(clock_handle, curr_time));
        	 _TRY(ismd_ret,result,ismd_vidsink_set_base_time,(vidsink_handle, curr_time));
        }
        case NERO_STC_VIDEO_MASTER:
        case NERO_STC_FREERUN:
        default:
        {
        	curr_time =0x00;
         	_TRY(ismd_ret,result,ismd_clock_set_time,(clock_handle, curr_time));
         	 result = stc->NeroSTCSetBaseTime(curr_time);
        	 _TRY(ismd_ret,result,ismd_vidsink_set_base_time,(vidsink_handle, curr_time));
        	 break;
        }
    }
    _TRY(ismd_ret, result , ismd_vidsink_set_state,(vidsink_handle,ISMD_DEV_STATE_PAUSE));
    _TRY(ismd_ret, result , ismd_dev_set_state,(viddec_handle, ISMD_DEV_STATE_PAUSE));
    result = NeroVideoDecoderPlay();
    if (NERO_SUCCESS !=result)
    {
 	   NERO_VIDEO_ERROR ("NeroAudioDecoderPlay fail !!! \n");
 	   goto error;
    }
    result = NeroIntelCE4x00VideoDecoderSend_new_segment();
    _TRY(ismd_ret,result ,NeroIntelCE4x00VideoDecoder_subscribe_events,());
    os_thread_create(&vid_evt_handler_thread, vid_evt_handler, this, 0, 0, "vid_evt_handler thread");

error:
    return (result);

}

/** Nero ISMD Video decoder to on fly reconfigure Video decoder  */

Nero_error_t NeroIntelCE4x00VideoDecoder::NeroVideoDecoderReconfigure(Nero_video_codec_t NeroVideoAlgo)
{
    NERO_VIDEO_NOTICE ("NeroVideoDecoderReconfigure  ... \n");
    Nero_error_t result = NERO_SUCCESS;
    return(result);
}

/** Nero ISMD Video decoder to on fly reconfigure Video decoder  */

Nero_error_t  NeroIntelCE4x00VideoDecoder::NeroVideoDecoderSetupPlane()
{

    NERO_VIDEO_NOTICE ("NeroVideoDecoderSetupPlane  ... \n");
    Nero_error_t result = NERO_SUCCESS;
    gdl_ret_t ret_gdl = GDL_SUCCESS;

    gdl_display_info_t         display;
    gdl_rectangle_t            rect;
    gdl_plane_id_t             plane;
    gdl_boolean_t              is_scaling_enabled;
    uint8_t                    alpha;

     is_scaling_enabled = GDL_TRUE;
     layer = GDL_PLANE_ID_UPP_B;

     if ((ret_gdl = gdl_init(0))!= GDL_SUCCESS)
     {
    	 NERO_VIDEO_ERROR("Failed To initialize GDL gdl_ret = %d \n", ret_gdl);
         result = NERO_ERROR_INTERNAL;
         goto error;
     }

     if ((ret_gdl = gdl_get_display_info(GDL_DISPLAY_ID_0, &display))!= GDL_SUCCESS)
     {
    	 NERO_VIDEO_ERROR("Failed at gdl_get_display_info gdl_ret = %d \n", ret_gdl);
         result = NERO_ERROR_INTERNAL;
         goto error;
     }

     if ((ret_gdl = gdl_plane_reset(layer))!= GDL_SUCCESS)
     {
    	 NERO_VIDEO_ERROR("Failed at gdl_plane_reset = %d \n", ret_gdl);
         result = NERO_ERROR_INTERNAL;
         goto error;
     }

     if ((ret_gdl = gdl_plane_config_begin(layer))!= GDL_SUCCESS)
     {
    	 NERO_VIDEO_ERROR("Failed at gdl_plane_config_begin = %d \n", ret_gdl);
         result = NERO_ERROR_INTERNAL;
         goto error;
     }

     rect.origin.x = 0;
     rect.origin.y = 0;
     rect.width    = display.tvmode.width;
     rect.height   = display.tvmode.height;

     NERO_VIDEO_NOTICE("origin.x = %d \n", rect.origin.x);
     NERO_VIDEO_NOTICE("origin.y = %d \n", rect.origin.y);
     NERO_VIDEO_NOTICE("rect.width = %d \n", rect.width);
     NERO_VIDEO_NOTICE("rect.height = %d \n", rect.height);

     if ((ret_gdl = gdl_plane_set_attr(GDL_PLANE_DST_RECT, &rect))!= GDL_SUCCESS)
     {
    	 NERO_VIDEO_ERROR("Failed at gdl_plane_set_attr = %d\n", ret_gdl);
         result = NERO_ERROR_INTERNAL;
         goto error;
     }

     if ((ret_gdl = gdl_plane_set_attr(GDL_PLANE_SCALE, &is_scaling_enabled))!= GDL_SUCCESS)
     {
    	 NERO_VIDEO_ERROR("Failed at gdl_plane_set_attr = %d\n", ret_gdl);
         result = NERO_ERROR_INTERNAL;
         goto error;
     }

     if ((ret_gdl = gdl_plane_config_end(GDL_FALSE))!= GDL_SUCCESS)
     {
    	 NERO_VIDEO_ERROR("Failed at gdl_plane_config_end = %d\n", ret_gdl);
         result = NERO_ERROR_INTERNAL;
         goto error;
     }
error:

    return (result);
}

/** Nero ISMD Video decoder to on fly reconfigure Video decoder  */

Nero_error_t  NeroIntelCE4x00VideoDecoder::NeroVideoDecoderResizeLayer(size_t w,size_t h,size_t x,size_t y)
{

    NERO_VIDEO_NOTICE ("NeroVideoDecoderResizeLayer  ... \n");
    Nero_error_t result = NERO_SUCCESS;
    ismd_result_t ismd_ret;

    printf ("Resize (org): w(%d)*h(%d) x(%d) y(%d)\n", w, h, x, y);
    ismd_vidsink_scale_params_t sp;
    memset(&sp,0,sizeof(sp));

    sp.crop_window.h_offset = 0;
    sp.crop_window.v_offset = 0;
    sp.crop_window.width = w;
    sp.crop_window.height = h;
    sp.crop_enable = 0;
    sp.dest_window.x=x;
    sp.dest_window.y=y;
    sp.dest_window.width = w;
    sp.dest_window.height = h;
    sp.aspect_ratio.numerator = 1;
    sp.aspect_ratio.denominator = 1;
    sp.scaling_policy = SCALE_TO_FIT;
    _TRY(ismd_ret, result , ismd_vidsink_set_global_scaling_params,(vidsink_handle,sp));
error:
    return (result);
}


Nero_error_t NeroIntelCE4x00VideoDecoder::NeroVideoDecoderUnInit()
{
    NERO_VIDEO_NOTICE ("NeroVideoDecoderUnInit  ... \n");
    Nero_error_t result = NERO_SUCCESS;
    ismd_result_t ismd_ret;
    /*_TRY(ismd_ret,pause_video_pipeline,());*/
    result = NeroVideoDecoderPause();
    if (NERO_SUCCESS !=result)
    {
    	NERO_VIDEO_ERROR ("NeroVideoDecoderPause fail !!! \n");
        goto error;
    }
    /*_TRY(ismd_ret,stop_video_pipeline,());*/
    result = NeroVideoDecoderStop();
    if (NERO_SUCCESS !=result)
    {
        NERO_VIDEO_ERROR ("NeroVideoDecoderStop fail !!! \n");
   	    goto error;
    }
    _TRY(ismd_ret,result ,NeroIntelCE4x00VideoDecoder_unsubscribe_events,());
    /*_TRY(ismd_ret,flush_video_pipeline,());*/
    result = NeroVideoDecoderFlush();
    if (NERO_SUCCESS !=result)
    {
        NERO_VIDEO_ERROR ("NeroVideoDecoderFlush fail !!! \n");
   	    goto error;
    }

     /*_TRY(ismd_ret,disconnect_video_pipeline,());*/

    /*_TRY(ismd_ret,close_video_pipeline,());*/
    result = NeroVideoDecoderClose();
    if (NERO_SUCCESS !=result)
    {
        NERO_VIDEO_ERROR ("NeroVideoDecoderFlush fail !!! \n");
 	    goto error;
    }
    result = NeroIntelCE4x00VideoDecoderInvalidateHandles();
    if (NERO_SUCCESS !=result)
    {
        NERO_VIDEO_ERROR ("NeroAudioDecoderPlay fail !!! \n");
        goto error;
    }
error:
    return(result);

}

/** Nero ISMD Video decoder fluch pipeline  */

Nero_error_t NeroIntelCE4x00VideoDecoder::NeroVideoDecoderClose()
{

    NERO_VIDEO_NOTICE ("NeroVideoDecoderClose  ... \n");
    Nero_error_t result = NERO_SUCCESS;
    ismd_result_t ismd_ret;
    _TRY(ismd_ret, result , ismd_dev_close, (viddec_handle));
    _TRY(ismd_ret, result , ismd_vidsink_close,(vidsink_handle));
    _TRY(ismd_ret, result , ismd_dev_close,(vidpproc_handle));
    _TRY(ismd_ret, result , ismd_dev_close,(vidrend_handle));

error:
     return (result);
}

/** Nero ISMD Video decoder fluch pipeline  */

Nero_error_t NeroIntelCE4x00VideoDecoder::NeroVideoDecoderFlush()
{
    NERO_VIDEO_NOTICE ("NeroVideoDecoderFlush  ... \n");
    Nero_error_t result = NERO_SUCCESS;
    ismd_result_t ismd_ret;
    _TRY(ismd_ret, result, ismd_dev_flush, (viddec_handle)  );
    _TRY(ismd_ret, result, ismd_dev_flush, (vidpproc_handle));
    _TRY(ismd_ret, result, ismd_dev_flush, (vidrend_handle) );

error:
    return (result);

}

/** Nero ISMD Video decoder fluch pipeline  */
Nero_error_t NeroIntelCE4x00VideoDecoder::NeroVideoDecoderStop()
{
    NERO_VIDEO_NOTICE ("NeroVideoDecoderStop  ... \n");
    Nero_error_t result = NERO_SUCCESS;
    ismd_result_t ismd_ret;
    _TRY(ismd_ret, result ,ismd_vidsink_set_state,(vidsink_handle,ISMD_DEV_STATE_STOP));
    _TRY(ismd_ret,result ,ismd_dev_set_state,(viddec_handle, ISMD_DEV_STATE_STOP));

error:
    return (result);
}

/** Nero ISMD Video decoder pause pipeline  */

Nero_error_t NeroIntelCE4x00VideoDecoder::NeroVideoDecoderPause()
{
    NERO_VIDEO_NOTICE ("NeroVideoDecoderPause  ... \n");
    Nero_error_t result = NERO_SUCCESS;
    ismd_result_t ismd_ret;
    Nero_stc_type_t stc_type = stc->NeroSTCGetType();
    _TRY(ismd_ret, result , ismd_vidsink_set_state,(vidsink_handle,ISMD_DEV_STATE_PAUSE));
    _TRY(ismd_ret, result , ismd_dev_set_state,(viddec_handle, ISMD_DEV_STATE_PAUSE));
    _TRY(ismd_ret,result, ismd_clock_get_time,(clock_handle, &orginal_clock_time));
    _TRY(ismd_ret,result ,ismd_dev_get_stream_base_time,(vidrend_handle, &original_base_time));
    if(stc_type == NERO_STC_VIDEO_MASTER)
    {
    	stc->NeroSTCSetBaseTime(original_base_time);
    }
error:

    return (result);
}

/** Nero ISMD Video decoder pause pipeline  */

Nero_error_t NeroIntelCE4x00VideoDecoder::NeroVideoDecoderPlay()
{

    NERO_VIDEO_NOTICE ("NeroVideoDecoderPlay  ... \n");
    Nero_error_t result = NERO_SUCCESS;
    ismd_result_t ismd_ret;
    ismd_dev_state_t state;
    Nero_stc_type_t stc_type = stc->NeroSTCGetType();
    _TRY(ismd_ret,result, ismd_dev_get_state,(vidrend_handle,&state));
    if(state != ISMD_DEV_STATE_PAUSE)
    {
       _TRY(ismd_ret,result ,ismd_vidsink_set_state,(vidsink_handle,ISMD_DEV_STATE_PAUSE));
    }
   _TRY(ismd_ret,result, ismd_clock_get_time,(clock_handle, &curr_clock));
   if(stc_type == NERO_STC_AUDIO_MASTER)
   {
       original_base_time=stc->NeroSTCGetBaseTime();
   }
   original_base_time+= curr_clock-orginal_clock_time+3000;

   _TRY(ismd_ret, result, ismd_vidsink_set_base_time,(vidsink_handle,original_base_time));

   _TRY(ismd_ret,result ,ismd_vidsink_set_state,(vidsink_handle,ISMD_DEV_STATE_PLAY));
   _TRY(ismd_ret,result ,ismd_dev_set_state,(viddec_handle, ISMD_DEV_STATE_PLAY));
   if(state != ISMD_DEV_STATE_PAUSE)
   {
      _TRY(ismd_ret,result ,ismd_vidsink_flush,(vidsink_handle));
   }
error:
    return (result);

}

Nero_error_t   NeroIntelCE4x00VideoDecoder::NeroVideoDecoderFeed (uint8_t* payload, size_t payload_size, uint64_t nrd_pts, bool discontinuity)
{

	Nero_error_t result = NERO_SUCCESS;
    ismd_result_t            ismd_ret;
    ismd_buffer_handle_t     bufin_handle;
    ismd_buffer_descriptor_t desc;
    int                      inject_size = 0;
    int                      temp_size = 0;
    uint8_t                 *ptr;
    ismd_es_buf_attr_t      *buf_attrs;
    ismd_pts_t pts_90Khz = (ismd_pts_t)(nrd_pts - PTS_START_VALUE) * 90LL;
    // Note that payload size may be greater than an ISMD buffer size.
    // So we need to cut them if necessary
    do {
        if ( (payload_size-inject_size) < INPUT_BUF_SIZE ) {
            temp_size = payload_size-inject_size;
        } else {
            temp_size = INPUT_BUF_SIZE;
        }
        ismd_ret = ismd_buffer_alloc(INPUT_BUF_SIZE, &bufin_handle);

        if(ismd_ret != ISMD_SUCCESS) {
            printf("ismd_buffer_alloc failed (%d) !\n", ismd_ret);
            result = NERO_ERROR_INTERNAL;
            goto error;
        }

        /* 1-READ_DESC */
        ismd_ret = ismd_buffer_read_desc(bufin_handle, &desc);
        if (ismd_ret != ISMD_SUCCESS) {
            printf("ismd_buffer_read_desc failed (%d) !\n", ismd_ret);
            result = NERO_ERROR_INTERNAL;
            goto error;
        }


        // map buffer physical address to a virtual address
        ptr = (uint8_t*)OS_MAP_IO_TO_MEM_NOCACHE(desc.phys.base, desc.phys.size);

        // fill the buffer with the payload
        OS_MEMCPY(ptr, (payload + inject_size), temp_size);

        // unmap buffer physical address from a virtual address
        OS_UNMAP_IO_FROM_MEM(ptr, desc.phys.size);

        desc.phys.level = temp_size;

        // set the attr
        buf_attrs = (ismd_es_buf_attr_t *) desc.attributes;
        buf_attrs->local_pts     = pts_90Khz;
        buf_attrs->original_pts  = pts_90Khz;
        buf_attrs->discontinuity = discontinuity;

        /* 2-UPDATE_DESC */
        ismd_ret = ismd_buffer_update_desc(bufin_handle, &desc);
        if (ismd_ret != ISMD_SUCCESS) {
            printf("ismd_buffer_update_desc failed (%d)\n", ismd_ret);
            result = NERO_ERROR_INTERNAL;
            goto error;
        }
        while (ismd_port_write(viddec_input_port_handle, bufin_handle) != ISMD_SUCCESS) {
            usleep(40);
        }

        inject_size += temp_size;
    } while ( inject_size < payload_size );

error:
	return (result);
}

NeroDecoderState_t NeroIntelCE4x00VideoDecoder::NeroVideoDecoderGetState()
{
    NERO_VIDEO_NOTICE ("NeroVideoDecoderGetState  ... \n");
    return (DecoderState);
}
uint64_t NeroIntelCE4x00VideoDecoder::NeroVideoDecoderGetLastPts()
{
	/*NERO_VIDEO_NOTICE ("NeroVideoDecoderGetLastPts  ... \n");*/
	ismd_result_t            ismd_ret;
	ismd_pts_t                           video_pts=0x00;
	ismd_vidrend_stream_position_info_t  video_position;
    ismd_ret = ismd_vidrend_get_stream_position(vidrend_handle, &video_position);
    if(ismd_ret == ISMD_ERROR_NO_DATA_AVAILABLE) {
       video_pts = 0x00;
    }
    else if(ismd_ret != ISMD_SUCCESS) {
    	NERO_VIDEO_ERROR("failed at ismd_vidrend_get_stream_position (%d)\n", ismd_ret);
    	video_pts = 0x00;
    }
    else {
       video_pts = (video_position.last_segment_pts/90LL);
   	/*NERO_VIDEO_NOTICE ("video_pts = %d  ... \n",video_pts);*/
    }
error:
    return((uint64_t)video_pts);
}
/* for testing */
int NeroIntelCE4x00VideoDecoder::NeroVideoDecoderGetport()
{
    NERO_VIDEO_NOTICE ("NeroVideoDecoderGetport  ... \n");
    return ((int)viddec_input_port_handle);
}

Info NeroIntelCE4x00VideoDecoder::Statut(void) const
{

	int evt = 0x00;
    pthread_mutex_lock((pthread_mutex_t*)&mutex_stock);
    evt = triggered_event;
    pthread_mutex_unlock((pthread_mutex_t*)&mutex_stock);
    NERO_AUDIO_NOTICE("NeroIntelCE4x00VideoDecoder :: triggered_event = %d \n",evt);
	return ((int)evt);
}
