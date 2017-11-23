#include "NeroIntelCE4x00AudioDecoder.h"

using namespace std;
/*************************************************************************************/
/********************************private methods**************************************/
/*************************************************************************************/

Nero_error_t NeroIntelCE4x00AudioDecoder::NeroIntelCE4x00AudioDecoderInvalidateHandles()
{

    NERO_AUDIO_NOTICE ("NeroIntelCE4x00AudioDecoderInvalidateHandles ... \n");
    Nero_error_t     result = NERO_SUCCESS;
    audio_handle            = ISMD_DEV_HANDLE_INVALID ;
    audio_input_port_handle = ISMD_PORT_HANDLE_INVALID;
    audio_processor         = ISMD_DEV_HANDLE_INVALID ;
    audio_output_handle     = ISMD_PORT_HANDLE_INVALID;
    clock_handle            = ISMD_CLOCK_HANDLE_INVALID;
    AudioDecoderState = NERO_DECODER_LAST;
    return(result);

}

Nero_error_t  NeroIntelCE4x00AudioDecoder::NeroIntelCE4x00AudioDecoder_EventSubscribe()
{
    NERO_AUDIO_NOTICE ("NeroIntelCE4x00AudioDecoder_EventSubscribe ... \n");
    Nero_error_t     result = NERO_SUCCESS;
    ismd_result_t ismd_ret = ISMD_SUCCESS;
    size_t event = NERO_EVENT_LAST;
       for(event = NERO_EVENT_FRAME_FLIPPED ;event <NERO_EVENT_LAST;event++) {
   	        switch(event)
   	        {
   	     //*************************************************************************
   	     // Heartbeat events - one from video and one from audio
   	     //*************************************************************************   	        }
	           case NERO_EVENT_FRAME_FLIPPED://-----------------------------------SYNC_IN
	           {
                   ismd_ret = ismd_audio_input_get_notification_event(audio_processor, audio_handle, ISMD_AUDIO_NOTIFY_DATA_RENDERED , &Nero_Events[event]);
                   if(ismd_ret != ISMD_SUCCESS) {
                       printf("failed at ismd_audio_input_get_notification_event on %d (ismd_evt = %d) : ismd_ret=%d\n",event, Nero_Events[event], ismd_ret);
                       assert(0);
                   }
                   break;
               }
	           case NERO_EVENT_UNDERFLOW:
	           {
	               ismd_ret = ismd_audio_input_get_notification_event(audio_processor, audio_handle, ISMD_AUDIO_NOTIFY_INPUT_EMPTY , &Nero_Events[event]);
	               if(ismd_ret != ISMD_SUCCESS) {
                       printf("failed at ismd_audio_input_get_notification_event on %d (ismd_evt = %d) : ismd_ret=%d\n",event, Nero_Events[event], ismd_ret);
	                   assert(0);
	               }
	               break;
	           }
	           case NERO_EVENT_UNDERFLOWRECOVRED:
	           {
	               ismd_ret = ismd_audio_input_get_notification_event(audio_processor, audio_handle, ISMD_AUDIO_NOTIFY_INPUT_RECOVERED , &Nero_Events[event]);
	               if(ismd_ret != ISMD_SUCCESS) {
                       printf("failed at ismd_audio_input_get_notification_event on %d (ismd_evt = %d) : ismd_ret=%d\n",event, Nero_Events[event], ismd_ret);
	                   assert(0);
	               }
	               break;
	           }
	           case NERO_EVENT_PTS_VALUE_EARLY:
	           {
	               ismd_ret = ismd_audio_input_get_notification_event(audio_processor, audio_handle, ISMD_AUDIO_NOTIFY_PTS_VALUE_EARLY , &Nero_Events[event]);
	               if(ismd_ret != ISMD_SUCCESS) {
                       printf("failed at ismd_audio_input_get_notification_event on %d (ismd_evt = %d) : ismd_ret=%d\n",event, Nero_Events[event], ismd_ret);
	                   assert(0);
	               }
	               break;
	           }
	           case NERO_EVENT_PTS_VALUE_LATE:
	           {
	               ismd_ret = ismd_audio_input_get_notification_event(audio_processor, audio_handle, ISMD_AUDIO_NOTIFY_PTS_VALUE_LATE , &Nero_Events[event]);
	               if(ismd_ret != ISMD_SUCCESS) {
                       printf("failed at ismd_audio_input_get_notification_event on %d (ismd_evt = %d) : ismd_ret=%d\n",event, Nero_Events[event], ismd_ret);
	                   assert(0);
	               }
	               break;
	           }
	           case NERO_EVENT_PTS_PTS_VALUE_RECOVERED:
	           {
	               ismd_ret = ismd_audio_input_get_notification_event(audio_processor, audio_handle, ISMD_AUDIO_NOTIFY_PTS_VALUE_RECOVERED , &Nero_Events[event]);
	               if(ismd_ret != ISMD_SUCCESS) {
                       printf("failed at ismd_audio_input_get_notification_event on %d (ismd_evt = %d) : ismd_ret=%d\n",event, Nero_Events[event], ismd_ret);
	                   assert(0);
	               }
	               break;
	           }
	           case NERO_EVENT_LAST:
	           default:
	           {
                  break;
	           }

	        }
     }
    return (result);
}
Nero_error_t  NeroIntelCE4x00AudioDecoder::NeroIntelCE4x00AudioDecoder_EventUnSubscribe()
{
    NERO_AUDIO_NOTICE ("NeroIntelCE4x00AudioDecoder_EventUnSubscribe ... \n");
    Nero_error_t     result = NERO_SUCCESS;
    ismd_result_t ismd_ret = ISMD_SUCCESS;
    size_t event = NERO_EVENT_LAST;
    for(event=0; event< NERO_EVENT_LAST ;event++) {
        Nero_Events[event]=ISMD_EVENT_HANDLE_INVALID;
        ismd_ret = ismd_event_free(Nero_Events[event]);
        if(ismd_ret != ISMD_SUCCESS) {
            printf("failed at ismd_event_free on %d (ismd_evt = %d)\n",event, Nero_Events[event]);
        }
    }
    return (result);
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
    clock_handle = NeroIntelCE4x00SystemClock::aquire();
    audio_processor = (ismd_audio_processor_t)NeroIntelCE4x00AudioProcessor::instance()->GetHandle();
    AudioDecoderState = NERO_DECODER_LAST;
}

/** Nero ISMD Audio decoder constructor */
NeroIntelCE4x00AudioDecoder::~NeroIntelCE4x00AudioDecoder()
{

    NERO_AUDIO_NOTICE ("NeroIntelCE4x00AudioDecoder killer ... \n");
    Nero_error_t     result = NERO_SUCCESS;
    int rc = 0x00;
    ismd_result_t ismd_ret = ISMD_SUCCESS;
    clock_handle = ISMD_CLOCK_HANDLE_INVALID;

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
    Nero_stc_type_t stc_type = NERO_STC_FREERUN;
    ismd_audio_format_t aud_fmt = NeroIntelCE4x00AudioDecoder_NERO2ISMD_codeRemap(NeroAudioAlgo);
    ismd_time_t curr_time = 0x00;
   /*_TRY(ismd_ret, build_audio, (audio_algo));*/
    ismd_audio_output_config_t audio_output;
    audio_output.ch_config = ISMD_AUDIO_STEREO;
    audio_output.out_mode = ISMD_AUDIO_OUTPUT_PCM;
    audio_output.sample_rate = 48000;
    audio_output.sample_size = 16;
    audio_output.stream_delay = 0;
    audio_output.ch_map = AUDIO_CHAN_CONFIG_2_CH;

    ismd_ret = ismd_audio_add_input_port(audio_processor, true, &audio_handle,
                                             &audio_input_port_handle);
    if (ismd_ret!=ISMD_SUCCESS) {
    	result = NERO_ERROR_INTERNAL;
        NERO_AUDIO_ERROR(" ismd_audio_add_input_port failed \n");
        goto error;
    }

    _TRY(ismd_ret,result,ismd_audio_input_set_data_format,(audio_processor, audio_handle, aud_fmt));
    _TRY(ismd_ret,result,ismd_audio_input_enable,(audio_processor, audio_handle));
    /* HDMI OUTPUT */
    _TRY(ismd_ret,result,ismd_audio_add_phys_output,(audio_processor, GEN3_HW_OUTPUT_HDMI, audio_output, &audio_output_handle));
    _TRY(ismd_ret,result,ismd_audio_output_set_channel_config,(audio_processor, audio_output_handle, ISMD_AUDIO_STEREO));
    _TRY(ismd_ret,result,ismd_audio_output_set_sample_size,(audio_processor, audio_output_handle, 16));
    _TRY(ismd_ret,result,ismd_audio_output_set_sample_rate,(audio_processor, audio_output_handle, 48000));
    _TRY(ismd_ret,result,ismd_audio_output_set_mode,(audio_processor, audio_output_handle, ISMD_AUDIO_OUTPUT_PCM));
    _TRY(ismd_ret,result,ismd_audio_output_set_external_bit_clock_div,(audio_processor, audio_output_handle, 12));

	ismd_ret = ismd_dev_set_clock(audio_handle, clock_handle);
    if (ismd_ret!=ISMD_SUCCESS) {
    	result = NERO_ERROR_INTERNAL;
        NERO_VIDEO_ERROR(" ismd_vidrend_set_timing_mode failed \n");
        assert(0);
    }

    ismd_ret = ismd_clock_get_time(clock_handle, &curr_time);
    if(ismd_ret != ISMD_SUCCESS) {
       printf("failed at ismd_clock_get_time\n");
       assert(0);
    }

    base_time = curr_time;

    ismd_ret = ismd_dev_set_stream_base_time(audio_handle, curr_time);
    if(ismd_ret != ISMD_SUCCESS) {
       printf("failed at ismd_dev_set_stream_base_time, audio_handle\n");
       assert(0);
    }

    _TRY(ismd_ret,result,ismd_audio_output_enable,(audio_processor, audio_output_handle));
   /*_TRY(ismd_ret,result, assign_audio_clock,());*/
    /* clock setting */
    NeroIntelCE4x00AudioDecoder_EventSubscribe();
error:
    return (result);

}

/** Nero ISMD Audio decoder to on fly reconfigure audio decoder  */

Nero_error_t NeroIntelCE4x00AudioDecoder::NeroAudioDecoderReconfigure(Nero_audio_codec_t NeroAudioAlgo)
{
    NERO_AUDIO_NOTICE ("NeroAudioDecoderReconfigure ... \n");
    Nero_error_t result = NERO_SUCCESS;
    ismd_result_t ismd_ret;

error:
    return(result);
}

Nero_error_t NeroIntelCE4x00AudioDecoder::NeroAudioDecoderUnInit()
{

    NERO_AUDIO_NOTICE ("NeroAudioDecoderUnInit ... \n");
    Nero_error_t result = NERO_SUCCESS;
    ismd_result_t ismd_ret;
    /*_TRY(ismd_ret,result,pause_audio_pipeline,());*/
    NeroIntelCE4x00AudioDecoder_EventUnSubscribe();
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

	/*_TRY(ismd_ret,result ,NeroIntelCE4x00AudioDecoder_unsubscribe_events,());*/
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


    AudioDecoderState = NERO_DECODER_LAST;

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
    if(AudioDecoderState != NERO_DECODER_STOP)
    {
        _TRY(ismd_ret, result ,ismd_dev_set_state,(audio_handle, ISMD_DEV_STATE_STOP));
        AudioDecoderState = NERO_DECODER_STOP;
    }
error:

    return (result);
}

/** Nero ISMD Audio decoder pause pipeline  */

Nero_error_t NeroIntelCE4x00AudioDecoder::NeroAudioDecoderPause()
{

    NERO_AUDIO_NOTICE ("NeroAudioDecoderPause ... \n");
    Nero_error_t result = NERO_SUCCESS;
    ismd_result_t ismd_ret;
    if (AudioDecoderState !=NERO_DECODER_PAUSE)
    {
        _TRY(ismd_ret, result ,ismd_dev_set_state,(audio_handle, ISMD_DEV_STATE_PAUSE));

        AudioDecoderState = NERO_DECODER_PAUSE;
    }
error:

    return (result);
}

/** Nero ISMD Audio decoder pause pipeline  */

Nero_error_t NeroIntelCE4x00AudioDecoder::NeroAudioDecoderPlay()
{

    NERO_AUDIO_NOTICE ("NeroAudioDecoderPlay ... \n");
    Nero_error_t result = NERO_SUCCESS;

    ismd_result_t ismd_ret;
    if ((AudioDecoderState != NERO_DECODER_PLAY))
    {
        base_time = NeroIntelCE4x00SystemClock::GetBaseTime();
        //set modules with new base_time
        ismd_ret = ismd_dev_set_stream_base_time(audio_handle, base_time);
        if(ismd_ret != ISMD_SUCCESS) {
           printf("failed at ismd_dev_set_stream_base_time, audio_handle\n");
        }

       _TRY(ismd_ret,result ,ismd_dev_set_state,(audio_handle,ISMD_DEV_STATE_PLAY));

       AudioDecoderState = NERO_DECODER_PLAY;
    }
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
    } while ( inject_size < payload_size);
error:
	return (result);
}

NeroDecoderState_t NeroIntelCE4x00AudioDecoder::NeroAudioDecoderGetState()
{
    NERO_AUDIO_NOTICE ("NeroAudioDecoderGetState ... \n");
    return (AudioDecoderState);
}

uint64_t NeroIntelCE4x00AudioDecoder::NeroAudioDecoderGetLastPts()
{
	/*NERO_VIDEO_NOTICE ("NeroAudioDecoderGetLastPts  ... \n");*/
	ismd_result_t            ismd_ret;
	ismd_pts_t                           audio_pts=0x00;
	ismd_audio_stream_position_info_t    audio_position;
	ismd_ret = ismd_audio_input_get_stream_position(audio_processor, audio_handle, &audio_position);
    if(ismd_ret == ISMD_ERROR_NO_DATA_AVAILABLE) {
    	NERO_AUDIO_ERROR("failed at ismd_audio_input_get_stream_position (%d)\n", ismd_ret);
    	audio_pts = 0x00;
    }
    else if(ismd_ret != ISMD_SUCCESS) {
    	NERO_AUDIO_ERROR("failed at ismd_audio_input_get_stream_position (%d)\n", ismd_ret);
    	audio_pts = 0x00;
    }
    else {
    	audio_pts = (audio_position.segment_time/90LL);
        if (audio_pts >= 0xFFFFFFFF)
        {
        	audio_pts=0x00;
        }
    }
    NERO_AUDIO_ERROR ("audio_pts = %d  ... \n",audio_pts);
error:
    return((uint64_t)audio_pts);
}


/* for testing */
int NeroIntelCE4x00AudioDecoder::NeroAudioDecoderGetport()
{
    NERO_AUDIO_NOTICE ("NeroIntelCE4x00AudioDecoderGetport ... \n");
    return ((int)audio_input_port_handle);

}

Nero_error_t   NeroIntelCE4x00AudioDecoder::NeroAudioDecoderEventWait(NeroEvents_t *event)
{
    ismd_event_t event_got;
    ismd_result_t ret_ismd = ISMD_SUCCESS;
	Nero_error_t result = NERO_SUCCESS;
	size_t evt = NERO_EVENT_LAST;

    ret_ismd = ismd_event_wait_multiple(Nero_Events,NERO_EVENT_LAST,NERO_EVENT_TIMEOUT, &event_got);

    if (ret_ismd == ISMD_ERROR_TIMEOUT) {
        result = NERO_ERROR_INTERNAL;
        goto exit;
    }

    for (evt = NERO_EVENT_FRAME_FLIPPED ; evt < NERO_EVENT_LAST;evt++)
    {
        if (Nero_Events[evt]==event_got)
        {
            switch (evt)
            {
                case NERO_EVENT_FRAME_FLIPPED:
                {
                	printf ("A- NERO_EVENT_FRAME_FLIPPED .. \n");
                    event->pts = NeroAudioDecoderGetLastPts();
                    event->header = (Nero_Event_Liste_t) evt;
                    break;
                }
                case NERO_EVENT_UNDERFLOW:
                {
                	printf ("A- NERO_EVENT_UNDERFLOW .. \n");
                    event->header = (Nero_Event_Liste_t) evt;
                    break;
                }
                case NERO_EVENT_UNDERFLOWRECOVRED:
                {
                	printf ("A-NERO_EVENT_UNDERFLOWRECOVRED .. \n");
                    event->header = (Nero_Event_Liste_t) evt;
                    break;
                }
                case NERO_EVENT_PTS_VALUE_EARLY:
                {
                	printf ("A-NERO_EVENT_PTS_VALUE_EARLY .. \n");
                    event->header = (Nero_Event_Liste_t) evt;
                    break;
                }
                case NERO_EVENT_PTS_VALUE_LATE:
                {
                	printf ("A-NERO_EVENT_PTS_VALUE_LATE .. \n");
                    event->header = (Nero_Event_Liste_t) evt;
                    break;
                }
                case NERO_EVENT_PTS_PTS_VALUE_RECOVERED:
                {
                	printf ("A-NERO_EVENT_PTS_PTS_VALUE_RECOVERED .. \n");
                    event->header = (Nero_Event_Liste_t) evt;
                    break;
                }
                case NERO_EVENT_LAST:
                default:
                    printf("A- Unknown event %d\n", evt);
                    event->header =(Nero_Event_Liste_t)evt;
                    break;
            }

            /* acknowledge the event */
            ret_ismd = ismd_event_acknowledge(event_got);
            if (ret_ismd!=ISMD_SUCCESS)
            {
                printf("Failed at ismd_event_acknowledge \n");
                result = NERO_ERROR_INTERNAL;
                goto exit;
            }
            break;
        }
    }
exit:
	return (NERO_SUCCESS);
}
