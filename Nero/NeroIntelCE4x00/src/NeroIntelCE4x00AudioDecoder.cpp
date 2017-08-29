#include "NeroIntelCE4x00AudioDecoder.h"
using namespace std;

/*************************************************************************
 * private:
**************************************************************************/

extern "C"
{
void *aud_evt_handler(void* args)
{
	   NeroIntelCE4x00AudioDecoder*  m_NeroIntelCE4x00AudioDecoder = (NeroIntelCE4x00AudioDecoder*)args;
	   ismd_result_t ismd_ret=ISMD_SUCCESS;
	   ismd_event_t triggered_event=ISMD_EVENT_HANDLE_INVALID;
	   int i = 0;
	   char* event_name = "ERROR : this evt_id is not a registered event !";
	   while(! m_NeroIntelCE4x00AudioDecoder->aud_evt_handler_thread_exit) {
	      ismd_ret=ismd_event_wait_multiple(m_NeroIntelCE4x00AudioDecoder->ismd_aud_evt_tab, AUDIO_NB_EVENT_TO_MONITOR, EVENT_TIMEOUT, &triggered_event);
	      if(ismd_ret!=ISMD_ERROR_TIMEOUT) {
	         if (ismd_ret!=ISMD_SUCCESS) {
	            printf("aud event waited failed (%d)\n", ismd_ret);
	         }
	         else {
	           /* printf("--> AUD EVT RECEIVED : %d (%s)\n", triggered_event, dbg_evt(triggered_event, AUDIO_EVT));
	            if(triggered_event == ISMD_AUDIO_NOTIFY_STREAM_END) {
	               audio_eos_reached = true;
	            }*/
	            ismd_ret=ismd_event_acknowledge(triggered_event);
	            if (ismd_ret!=ISMD_SUCCESS){
	               printf("ismd_event_acknowledge failed (%d)\n", ismd_ret);
	               OS_ASSERT(0);
	            }
	         }
	      }
	      if(triggered_event !=ISMD_EVENT_HANDLE_INVALID)
	      {
            pthread_mutex_lock(&m_NeroIntelCE4x00AudioDecoder->mutex_stock);
	    	m_NeroIntelCE4x00AudioDecoder->triggered_event = triggered_event;
	        pthread_mutex_unlock(&m_NeroIntelCE4x00AudioDecoder->mutex_stock);
	        m_NeroIntelCE4x00AudioDecoder->Notify();
	      }
	      usleep(10000);
	   }
	   printf("exit aud_evt_handler \n");
	   return NULL;
}
}
Nero_error_t NeroIntelCE4x00AudioDecoder::NeroIntelCE4x00AudioDecoderInvalidateHandles()
{

    NERO_AUDIO_NOTICE ("NeroIntelCE4x00AudioDecoderInvalidateHandles ... \n");
    Nero_error_t     result = NERO_SUCCESS;
    audio_handle            = ISMD_DEV_HANDLE_INVALID ;
    audio_input_port_handle = ISMD_PORT_HANDLE_INVALID;
    audio_processor         = ISMD_DEV_HANDLE_INVALID ;
    audio_output_handle     = ISMD_PORT_HANDLE_INVALID;
    clock_handle            = ISMD_DEV_HANDLE_INVALID;

    return(result);

}

Nero_error_t NeroIntelCE4x00AudioDecoder::NeroIntelCE4x00AudioDecoderSend_new_segment()
{

    NERO_AUDIO_NOTICE ("NeroIntelCE4x00AudioDecoderSend_new_segment ... \n");
    Nero_error_t     result = NERO_SUCCESS;
    ismd_newsegment_tag_t newsegment_data;
    ismd_buffer_handle_t carrier_buffer;
    ismd_result_t ismd_ret = ISMD_SUCCESS;

    newsegment_data.linear_start = 0;
    newsegment_data.start = ISMD_NO_PTS;
    newsegment_data.stop = ISMD_NO_PTS;
    newsegment_data.requested_rate = 10000;
    newsegment_data.applied_rate = ISMD_NORMAL_PLAY_RATE;
    newsegment_data.rate_valid = true;

    _TRY(ismd_ret,result,ismd_buffer_alloc,(0, &carrier_buffer));
    _TRY(ismd_ret,result ,ismd_tag_set_newsegment,(carrier_buffer, newsegment_data));
    _TRY(ismd_ret,result, ismd_port_write,(audio_input_port_handle, carrier_buffer));
 error:
     return (result);
}

ismd_result_t NeroIntelCE4x00AudioDecoder::NeroIntelCE4x00AudioDecoder_subscribe_events()
{
   ismd_result_t ismd_ret;
   int i=0;
   NERO_AUDIO_NOTICE ("NeroIntelCE4x00AudioDecoder_subscribe_events ...  \n");
   // subscribe audio events
   if(AUDIO_NB_EVENT_TO_MONITOR>0) {
      for(i=0;i<AUDIO_NB_EVENT_TO_MONITOR;i++) {
    	  printf ("getting notification for event %d \n",aud_evt_to_monitor[i]);
         ismd_ret = ismd_audio_input_get_notification_event(audio_processor, audio_handle, aud_evt_to_monitor[i], &ismd_aud_evt_tab[i]);
         if(ismd_ret != ISMD_SUCCESS) {
            printf("failed at ismd_audio_input_get_notification_event on %d (ismd_evt = %d) : ismd_ret=%d\n",i, aud_evt_to_monitor[i], ismd_ret);
            assert(0);
         }
         //printf("get_notification_event evt %d (%s) OK \n", ismd_aud_evt_tab[i],dbg_evt(ismd_aud_evt_tab[i],AUDIO_EVT));
      }
   }
   return (ismd_ret);
}

ismd_result_t NeroIntelCE4x00AudioDecoder::NeroIntelCE4x00AudioDecoder_unsubscribe_events()
{
  ismd_result_t ismd_ret;
  int i=0;

  if(AUDIO_NB_EVENT_TO_MONITOR>0) {
     for(i=0;i<AUDIO_NB_EVENT_TO_MONITOR;i++) {
        ismd_aud_evt_tab[i]=ISMD_EVENT_HANDLE_INVALID;
        ismd_ret = ismd_event_free(ismd_aud_evt_tab[i]);
        if(ismd_ret != ISMD_SUCCESS) {
           printf("failed at ismd_event_free on %d (ismd_evt = %d)\n",i, ismd_aud_evt_tab[i]);
           assert(0);
        }
     }
  }
  return (ismd_ret);
}


ismd_audio_format_t NeroIntelCE4x00AudioDecoder::NeroIntelCE4x00AudioDecoder_NERO2ISMD_codeRemap(Nero_audio_codec_t NeroAudioAlgo)
{

    NERO_AUDIO_NOTICE ("NeroIntelCE4x00AudioDecoder_NERO2ISMD_codeRemap ... %d \n",NeroAudioAlgo);
    ismd_audio_format_t aud_fmt = ISMD_AUDIO_MEDIA_FMT_AAC;
    switch (NeroAudioAlgo)
    {

    case NERO_CODEC_AUDIO_PCM          :/**< - stream is raw linear PCM data*/
    {
    	aud_fmt =ISMD_AUDIO_MEDIA_FMT_PCM;
    	break;
    }
    case NERO_CODEC_AUDIO_DVD_PCM      :/**< - stream is linear PCM data coming from a DVD */
    {
	    aud_fmt =ISMD_AUDIO_MEDIA_FMT_DVD_PCM;
	    break;
    }
    case NERO_CODEC_AUDIO_BLURAY_PCM   :/**< - stream is linear PCM data coming from a BD (HDMV Audio) */
    {
	    aud_fmt =ISMD_AUDIO_MEDIA_FMT_BLURAY_PCM;
	    break;
    }
    case NERO_CODEC_AUDIO_MPEG         :/**< - stream uses MPEG-1 or MPEG-2 BC algorithm*/
    {
	    aud_fmt =ISMD_AUDIO_MEDIA_FMT_MPEG;
	    break;
    }
    case NERO_CODEC_AUDIO_AAC          :/**< - stream uses MPEG-2 or MPEG-4 AAC algorithm*/
    {
	    aud_fmt =ISMD_AUDIO_MEDIA_FMT_AAC;
	    break;
    }
    case NERO_CODEC_AUDIO_AAC_LOAS     :/**< - stream uses MPEG-2 or MPEG-4 AAC algorithm with LOAS header format*/
    {
	    aud_fmt =ISMD_AUDIO_MEDIA_FMT_AAC_LOAS;
	    break;
    }
    case NERO_CODEC_AUDIO_DD           :/**< - stream uses Dolby Digital (AC3) algorithm*/
    {
	    aud_fmt =ISMD_AUDIO_MEDIA_FMT_DD;
	    break;
    }
    case NERO_CODEC_AUDIO_DD_PLUS      :/**< - stream uses Dolby Digital Plus (E-AC3) algorithm*/
    {
	    aud_fmt =ISMD_AUDIO_MEDIA_FMT_DD_PLUS;
	    break;
    }
    case NERO_CODEC_AUDIO_TRUE_HD      :/**< - stream uses Dolby TrueHD algorithm*/
    {
	    aud_fmt =ISMD_AUDIO_MEDIA_FMT_TRUE_HD;
	    break;
    }
    case NERO_CODEC_AUDIO_DTS_HD       :/**< - stream uses DTS High Definition audio algorithm */
    {
	    aud_fmt =ISMD_AUDIO_MEDIA_FMT_DTS_HD;
	    break;
    }
    case NERO_CODEC_AUDIO_DTS_HD_HRA   :/**< - DTS-HD High Resolution Audio */
    {
	    aud_fmt =ISMD_AUDIO_MEDIA_FMT_DTS_HD_HRA;
	    break;
    }
    case NERO_CODEC_AUDIO_DTS_HD_MA    :/**< - DTS-HD Master Audio */
    {
	    aud_fmt =ISMD_AUDIO_MEDIA_FMT_DTS_HD_MA;
	    break;
    }
    case NERO_CODEC_AUDIO_DTS          :/**< - stream uses DTS  algorithm*/
    {
	    aud_fmt =ISMD_AUDIO_MEDIA_FMT_DTS;
	    break;
    }
    case NERO_CODEC_AUDIO_DTS_LBR      :/**< - stream uses DTS Low BitRate decode algorithm*/
    {
	    aud_fmt =ISMD_AUDIO_MEDIA_FMT_DTS_LBR;
	    break;
    }
    case NERO_CODEC_AUDIO_DTS_BC       :/**< - stream uses DTS broadcast decode algorithm*/
    {
	    aud_fmt =ISMD_AUDIO_MEDIA_FMT_DTS_BC;
	    break;
    }
    case NERO_CODEC_AUDIO_WM9          :/**< - stream uses Windows Media 9 algorithm*/
    {
	    aud_fmt =ISMD_AUDIO_MEDIA_FMT_WM9;
	    break;
    }
    case NERO_CODEC_AUDIO_DDC          :/**< - stream uses DD/DDP algorithm and decoded by DDC decoder*/
    {
	    aud_fmt =ISMD_AUDIO_MEDIA_FMT_DDC;
	    break;
    }
    case NERO_CODEC_AUDIO_DDT          :/**< - stream uses AAC(up to AAC-HE V2)algorithm and decoded by DDT decoder*/
    {
	    aud_fmt =ISMD_AUDIO_MEDIA_FMT_DDT;
	    break;
    }
    case NERO_CODEC_AUDIO_DRA          :/**< - stream uses Digital Rise Audio algorithm*/
    {
	    aud_fmt =ISMD_AUDIO_MEDIA_FMT_DRA;
	    break;
    }
    case NERO_CODEC_AUDIO_COUNT        :/**< - Count of enum members.*/
    {
	    aud_fmt =ISMD_AUDIO_MEDIA_FMT_COUNT;
	    break;
    }
    case  NERO_CODEC_AUDIO_INVALID:      /**< - Invalid or Unknown algorithm*/
    default:
    {
    	aud_fmt =ISMD_AUDIO_MEDIA_FMT_INVALID;
    }
    }
    NERO_AUDIO_NOTICE ("ismd_audio_format_t ... %d \n",aud_fmt);
    return ((ismd_audio_format_t) aud_fmt);
}


/*************************************************************************
 * public:
**************************************************************************/

/** Nero ISMD Audio decoder constructor */
NeroIntelCE4x00AudioDecoder::NeroIntelCE4x00AudioDecoder()
{

    NERO_AUDIO_NOTICE ("NeroIntelCE4x00AudioDecoder creator  ... \n");
    Nero_error_t     result = NERO_SUCCESS;
    ismd_result_t ismd_ret = ISMD_SUCCESS;
    result = NeroIntelCE4x00AudioDecoderInvalidateHandles();
    internal_clk = true;
    ismd_ret = ismd_clock_alloc(ISMD_CLOCK_TYPE_FIXED, &clock_handle);
    if (ISMD_SUCCESS != ismd_ret)
    {
	    result = NERO_ERROR_INTERNAL;
	}
    audio_processor = (ismd_audio_processor_t)NeroIntelCE4x00AudioProcessor::instance()->GetHandle();
    aud_evt_to_monitor = {ISMD_AUDIO_NOTIFY_INPUT_FULL, ISMD_AUDIO_NOTIFY_INPUT_EMPTY, ISMD_AUDIO_NOTIFY_DATA_RENDERED };
    ismd_aud_evt_tab = {0,};
    triggered_event = 0x00;
    aud_evt_handler_thread_exit = false;
    if (pthread_mutex_init(&mutex_stock, NULL) != 0)
    {
        result = NERO_ERROR_INTERNAL;
        NERO_AUDIO_ERROR("mopal_fpout_pdd_data.dev.led_mutex  init failed \n");
    }
}


/** Nero ISMD Audio decoder constructor */
NeroIntelCE4x00AudioDecoder::NeroIntelCE4x00AudioDecoder(NeroSTC* NeroSTC_ptr)
{

    NERO_AUDIO_NOTICE ("NeroIntelCE4x00AudioDecoder creator  ... \n");
    Nero_error_t     result = NERO_SUCCESS;
    ismd_result_t ismd_ret = ISMD_SUCCESS;
    int rc=0;
    result = NeroIntelCE4x00AudioDecoderInvalidateHandles();
    internal_clk = false;
    clock_handle = (ismd_clock_t)NeroSTC_ptr->NeroSTCGetClock();
    stc = NeroSTC_ptr;
    audio_processor = (ismd_audio_processor_t)NeroIntelCE4x00AudioProcessor::instance()->GetHandle();
    aud_evt_to_monitor = {ISMD_AUDIO_NOTIFY_INPUT_FULL, ISMD_AUDIO_NOTIFY_INPUT_EMPTY, ISMD_AUDIO_NOTIFY_DATA_RENDERED };
    ismd_aud_evt_tab = {0,};
    triggered_event = 0x00;
    aud_evt_handler_thread_exit = false;
    if (pthread_mutex_init(&mutex_stock, NULL) != 0)
    {
        result = NERO_ERROR_INTERNAL;
        NERO_AUDIO_ERROR("mutex_stock  init failed \n");
    }
}

/** Nero ISMD Audio decoder constructor */
NeroIntelCE4x00AudioDecoder::~NeroIntelCE4x00AudioDecoder()
{

    NERO_AUDIO_NOTICE ("NeroIntelCE4x00AudioDecoder killer ... \n");
    Nero_error_t     result = NERO_SUCCESS;
    int rc = 0x00;
    ismd_result_t ismd_ret = ISMD_SUCCESS;
    aud_evt_handler_thread_exit = true;
    if(pthread_mutex_destroy(&mutex_stock)!=0)
    {
        result = NERO_ERROR_INTERNAL;
        NERO_AUDIO_ERROR("mutex_stock mutex destruction  fail ... (%d) \n", result);
    }
    ismd_ret = ismd_clock_alloc(ISMD_CLOCK_TYPE_FIXED, &clock_handle);
    if (ISMD_SUCCESS != ismd_ret)
    {
	    result = NERO_ERROR_INTERNAL;
	}
    stc = NULL;
    result = NeroIntelCE4x00AudioDecoderInvalidateHandles();

}

/** Nero ISMD Audio decoder to init and start the decoder  */
Nero_error_t NeroIntelCE4x00AudioDecoder::NeroAudioDecoderInit (Nero_audio_codec_t NeroAudioAlgo)
{
    /* audio */
    NERO_AUDIO_NOTICE ("NeroAudioDecoderInit ... \n");
    Nero_error_t result = NERO_SUCCESS;
    ismd_result_t ismd_ret;
    int time_valid = 0x01;
    orginal_clock_time = 0x00;
    Nero_stc_type_t stc_type = NERO_STC_FREERUN;
    ismd_audio_format_t aud_fmt = NeroIntelCE4x00AudioDecoder_NERO2ISMD_codeRemap(NeroAudioAlgo);
    ismd_time_t curr_time = 0x00;
   /*_TRY(ismd_ret, build_audio, (audio_algo));*/
    ismd_audio_output_config_t audio_output;
    audio_output.ch_config = ISMD_AUDIO_STEREO;
    audio_output.out_mode = ISMD_AUDIO_OUTPUT_PCM;
    audio_output.sample_rate = 48000;
    audio_output.sample_size = 24;
    audio_output.stream_delay = 0;
    audio_output.ch_map = AUDIO_CHAN_CONFIG_2_CH;
    //ismd_audio_input_set_passthrough
    _TRY(ismd_ret,result,ismd_audio_add_input_port,(audio_processor, false, &audio_handle, &audio_input_port_handle));
    _TRY(ismd_ret,result,ismd_audio_input_set_data_format,(audio_processor, audio_handle, aud_fmt));
    _TRY(ismd_ret,result,ismd_audio_input_enable,(audio_processor, audio_handle));
    /* HDMI OUTPUT */
    _TRY(ismd_ret,result,ismd_audio_add_phys_output,(audio_processor, GEN3_HW_OUTPUT_HDMI, audio_output, &audio_output_handle));
    _TRY(ismd_ret,result,ismd_audio_output_set_channel_config,(audio_processor, audio_output_handle, ISMD_AUDIO_STEREO));
    _TRY(ismd_ret,result,ismd_audio_output_set_sample_size,(audio_processor, audio_output_handle, 16));
    _TRY(ismd_ret,result,ismd_audio_output_set_sample_rate,(audio_processor, audio_output_handle, 48000));
    _TRY(ismd_ret,result,ismd_audio_output_set_mode,(audio_processor, audio_output_handle, ISMD_AUDIO_OUTPUT_PCM));
    _TRY(ismd_ret,result,ismd_audio_output_set_external_bit_clock_div,(audio_processor, audio_output_handle, 12));
    _TRY(ismd_ret,result,ismd_audio_output_enable,(audio_processor, audio_output_handle));
    _TRY(ismd_ret,result,ismd_dev_set_state,(audio_handle,ISMD_DEV_STATE_PAUSE));
   /*_TRY(ismd_ret,result, assign_audio_clock,());*/

    _TRY(ismd_ret,result,ismd_dev_set_clock,(audio_handle, clock_handle));
   /*_TRY(ismd_ret,result, connect_audio_pipeline,());*/

   /*_TRY(ismd_ret,result, configure_audio_clocks,(0, 1));*/
	_TRY(ismd_ret,result,ismd_clock_get_time,(clock_handle, &curr_time));
    stc_type = stc->NeroSTCGetType();
        switch(stc_type)
        {
            case NERO_STC_VIDEO_MASTER:
            {
            	/*_TRY(ismd_ret,result,ismd_clock_get_time,(clock_handle, &curr_time));*/
            	curr_time = stc->NeroSTCGetBaseTime();
            	_TRY(ismd_ret,result,ismd_clock_set_time,(clock_handle, curr_time));
            	 _TRY(ismd_ret,result,ismd_dev_set_stream_base_time,(audio_handle, curr_time));
            }
            case NERO_STC_AUDIO_MASTER:
            case NERO_STC_FREERUN:
            default:
            {
            	curr_time =0x00;
            	_TRY(ismd_ret,result,ismd_clock_set_time,(clock_handle, curr_time));
            	 result = stc->NeroSTCSetBaseTime(curr_time);
            	 _TRY(ismd_ret,result,ismd_dev_set_stream_base_time,(audio_handle, curr_time));
            	 break;
            }
        }
        _TRY(ismd_ret,result ,ismd_dev_set_state,(audio_handle,ISMD_DEV_STATE_PAUSE));
    /*if(time_valid)
    {
         _TRY(ismd_ret,result,ismd_clock_set_time,(clock_handle, curr_time));
	}
    else
    {
	    _TRY(ismd_ret,result,ismd_clock_get_time,(clock_handle, &curr_time));
	}

    _TRY(ismd_ret,result,ismd_clock_set_time,(clock_handle, curr_time));
   _TRY(ismd_ret,result,ismd_dev_set_stream_base_time,(audio_handle, curr_time));
   /*_TRY(ismd_ret,result, start_audio_pipeline,());    */

   result = NeroAudioDecoderPlay();
   if (NERO_SUCCESS !=result)
   {
	   NERO_AUDIO_ERROR ("NeroAudioDecoderPlay fail !!! \n");
	   goto error;
   }
   result = NeroIntelCE4x00AudioDecoderSend_new_segment();

   _TRY(ismd_ret,result ,NeroIntelCE4x00AudioDecoder_subscribe_events,());
   os_thread_create(&aud_evt_handler_thread, aud_evt_handler, this, 0, 0, "aud_evt_handler thread");
error:
    return (result);

}

/** Nero ISMD Audio decoder to on fly reconfigure audio decoder  */

Nero_error_t NeroIntelCE4x00AudioDecoder::NeroAudioDecoderReconfigure(Nero_audio_codec_t NeroAudioAlgo)
{
    NERO_AUDIO_NOTICE ("NeroAudioDecoderReconfigure ... \n");
    Nero_error_t result = NERO_SUCCESS;
    ismd_result_t ismd_ret;
    ismd_audio_format_t aud_fmt = NeroIntelCE4x00AudioDecoder_NERO2ISMD_codeRemap(NeroAudioAlgo);
    NERO_AUDIO_NOTICE ("NeroAudioDecoderReconfigure ...(%d) \n",aud_fmt);
    _TRY(ismd_ret, result ,ismd_dev_set_state,(audio_handle, ISMD_DEV_STATE_STOP));
    _TRY(ismd_ret, result ,ismd_dev_flush,(audio_handle));
    _TRY(ismd_ret, result ,ismd_audio_input_set_data_format,(audio_processor, audio_handle, aud_fmt));
    //Set up the input again with the new info.
    _TRY(ismd_ret, result ,ismd_dev_set_state,(audio_handle, ISMD_DEV_STATE_PLAY));

error:
    return(result);
}

Nero_error_t NeroIntelCE4x00AudioDecoder::NeroAudioDecoderUnInit()
{

    NERO_AUDIO_NOTICE ("NeroAudioDecoderUnInit ... \n");
    Nero_error_t result = NERO_SUCCESS;
    ismd_result_t ismd_ret;
    /*_TRY(ismd_ret,result,pause_audio_pipeline,());*/
    result = NeroAudioDecoderPause();
    if (NERO_SUCCESS !=result)
    {
    	NERO_AUDIO_ERROR ("NeroAudioDecoderPause fail !!! \n");
        goto error;
    }
    /*_TRY(ismd_ret,result,stop_audio_pipeline,());*/
    result = NeroAudioDecoderStop();
    if (NERO_SUCCESS !=result)
    {
    	NERO_AUDIO_ERROR ("NeroAudioDecoderStop fail !!! \n");
        goto error;
    }

	_TRY(ismd_ret,result ,NeroIntelCE4x00AudioDecoder_unsubscribe_events,());
    /*_TRY(ismd_ret,result,flush_audio_pipeline,());*/
    result = NeroAudioDecoderFlush();
    if (NERO_SUCCESS !=result)
    {
    	NERO_AUDIO_ERROR ("NeroAudioDecoderFlush fail !!! \n");
        goto error;
    }
   /*_TRY(ismd_ret,result,disconnect_audio_pipeline,());*/
   /*_TRY(ismd_ret,result,close_audio_pipeline,());*/
    result = NeroAudioDecoderClose();
    if (NERO_SUCCESS !=result)
    {
    	NERO_AUDIO_ERROR ("NeroAudioDecoderClose fail !!! \n");
        goto error;
    }
    result = NeroIntelCE4x00AudioDecoderInvalidateHandles();

error:
    return(result);

}

/** Nero ISMD Audio decoder fluch pipeline  */

Nero_error_t NeroIntelCE4x00AudioDecoder::NeroAudioDecoderClose()
{

    NERO_AUDIO_NOTICE ("NeroAudioDecoderClose ... \n");
    Nero_error_t result = NERO_SUCCESS;
    ismd_result_t ismd_ret;

    _TRY(ismd_ret,result, ismd_dev_close,(audio_handle));


    AudioDecoderState = NERO_AUDIO_LAST;

error:
     return (result);

}

/** Nero ISMD Audio decoder fluch pipeline  */

Nero_error_t NeroIntelCE4x00AudioDecoder::NeroAudioDecoderFlush()
{

    NERO_AUDIO_NOTICE ("NeroAudioDecoderFlush ... \n");
    Nero_error_t result = NERO_SUCCESS;
    ismd_result_t ismd_ret;
    _TRY(ismd_ret,result,ismd_dev_flush,(audio_handle));

error:
    return (result);

}

/** Nero ISMD Audio decoder fluch pipeline  */
Nero_error_t NeroIntelCE4x00AudioDecoder::NeroAudioDecoderStop()
{

    NERO_AUDIO_NOTICE ("NeroAudioDecoderStop ... \n");
    Nero_error_t result = NERO_SUCCESS;
    ismd_result_t ismd_ret;
    _TRY(ismd_ret, result ,ismd_dev_set_state,(audio_handle, ISMD_DEV_STATE_STOP));

error:
    AudioDecoderState = NERO_AUDIO_STOP;
    return (result);
}

/** Nero ISMD Audio decoder pause pipeline  */

Nero_error_t NeroIntelCE4x00AudioDecoder::NeroAudioDecoderPause()
{

    NERO_AUDIO_NOTICE ("NeroAudioDecoderPause ... \n");
    Nero_error_t result = NERO_SUCCESS;
    ismd_result_t ismd_ret;
    Nero_stc_type_t stc_type = stc->NeroSTCGetType();
    _TRY(ismd_ret, result ,ismd_dev_set_state,(audio_handle, ISMD_DEV_STATE_PAUSE));
    _TRY(ismd_ret,result, ismd_clock_get_time,(clock_handle, &orginal_clock_time));
    _TRY(ismd_ret,result ,ismd_dev_get_stream_base_time,(audio_handle, &original_base_time));
    if(stc_type == NERO_STC_AUDIO_MASTER)
    {
    	stc->NeroSTCSetBaseTime(original_base_time);
    }

    AudioDecoderState = NERO_AUDIO_PAUSE;

error:

    return (result);
}

/** Nero ISMD Audio decoder pause pipeline  */

Nero_error_t NeroIntelCE4x00AudioDecoder::NeroAudioDecoderPlay()
{

    NERO_AUDIO_NOTICE ("NeroAudioDecoderPlay ... \n");
    Nero_error_t result = NERO_SUCCESS;
    ismd_result_t ismd_ret;
    Nero_stc_type_t stc_type = stc->NeroSTCGetType();


    ismd_dev_state_t state;
   _TRY(ismd_ret,result, ismd_dev_get_state,(audio_handle,&state));
    if(state != ISMD_DEV_STATE_PAUSE)
    {
       _TRY(ismd_ret,result ,ismd_dev_set_state,(audio_handle,ISMD_DEV_STATE_PAUSE));
    }
   _TRY(ismd_ret,result, ismd_clock_get_time,(clock_handle, &curr_clock));
   if(stc_type == NERO_STC_VIDEO_MASTER)
   {
       original_base_time=stc->NeroSTCGetBaseTime();
   }
   original_base_time += curr_clock-orginal_clock_time+3000;
   _TRY(ismd_ret, result, ismd_dev_set_stream_base_time,(audio_handle,original_base_time));

   _TRY(ismd_ret,result ,ismd_dev_set_state,(audio_handle,ISMD_DEV_STATE_PLAY));
   AudioDecoderState = NERO_AUDIO_PLAY;

error:
    return (result);

}

Nero_error_t  NeroIntelCE4x00AudioDecoder::NeroAudioDecoderFeed (uint8_t* payload, size_t payload_size, uint64_t nrd_pts, bool discontinuity)
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
        while (ismd_port_write(audio_input_port_handle, bufin_handle) != ISMD_SUCCESS) {
            usleep(40);
        }

        inject_size += temp_size;
    } while ( inject_size < payload_size );

error:
	return (result);
}

NeroDecoderState_t NeroIntelCE4x00AudioDecoder::NeroAudioDecoderGetState()
{
    NERO_AUDIO_NOTICE ("NeroAudioDecoderGetState ... \n");
    return (AudioDecoderState);
}


/* for testing */
int NeroIntelCE4x00AudioDecoder::NeroAudioDecoderGetport()
{
    NERO_AUDIO_NOTICE ("NeroIntelCE4x00AudioDecoderGetport ... \n");
    return ((int)audio_input_port_handle);

}

Info NeroIntelCE4x00AudioDecoder::Statut(void) const
{

	int evt = 0x00;
    pthread_mutex_lock((pthread_mutex_t*)&mutex_stock);
    evt = triggered_event;
    pthread_mutex_unlock((pthread_mutex_t*)&mutex_stock);
    NERO_AUDIO_NOTICE("NeroIntelCE4x00AudioDecoder :: triggered_event = %d \n",evt);
	return ((int)evt);
}

