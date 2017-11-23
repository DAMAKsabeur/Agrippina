#include "NeroIntelCE4x00VideoDecoder.h"

using namespace std;


/*************************************************************************
 * private:
**************************************************************************/
Nero_error_t  NeroIntelCE4x00VideoDecoder::NeroIntelCE4x00VideoDecoder_EventSubscribe()
{
    NERO_AUDIO_NOTICE ("NeroIntelCE4x00VideoDecoder_EventSubscribe ... \n");
    Nero_error_t     result = NERO_SUCCESS;
    size_t event = NERO_EVENT_LAST;
    ismd_result_t ismd_ret = ISMD_SUCCESS;
    for (event =NERO_EVENT_FRAME_FLIPPED ; event < NERO_EVENT_LAST ; event++)
    {
        switch(event) {
           case NERO_EVENT_FRAME_FLIPPED://-----------------------------------SYNC_IN
              ismd_ret = ismd_vidrend_get_frame_flipped_event(vidrend_handle, &Nero_Events[event]);
              if(ismd_ret != ISMD_SUCCESS) {
                 printf("failed at ismd_vidrend_get_frame_flipped_event\n");
                 assert(0);
              }
              break;

           case NERO_EVENT_UNDERFLOW:
              ismd_ret = ismd_vidrend_get_underflow_event(vidrend_handle, &Nero_Events[event]);
              if(ismd_ret != ISMD_SUCCESS) {
                 printf("failed at ismd_vidrend_get_underflow_event\n");
                 assert(0);
              }
              break;

           case NERO_EVENT_UNDERFLOWRECOVRED :
              ismd_ret = ismd_vidrend_get_underflow_recovered_event(vidrend_handle, &Nero_Events[event]);
              if(ismd_ret != ISMD_SUCCESS) {
                 printf("failed at ismd_vidrend_get_underflow_recovered_event  \n");
                 assert(0);
              }
              break;
              default : printf("The vidrend event %d can not be subscribed yet \n", Nero_Events[event]);
        }
     }

    return (result);
}
Nero_error_t  NeroIntelCE4x00VideoDecoder::NeroIntelCE4x00VideoDecoder_EventUnSubscribe()
{
    NERO_AUDIO_NOTICE ("NeroIntelCE4x00VideoDecoder_EventUnSubscribe ... \n");
    Nero_error_t     result = NERO_SUCCESS;
    size_t event = NERO_EVENT_LAST;
    ismd_result_t ismd_ret = ISMD_SUCCESS;
    for(event =NERO_EVENT_FRAME_FLIPPED ;event < NERO_EVENT_LAST; event++)
    {
        Nero_Events[event]=ISMD_EVENT_HANDLE_INVALID;
        ismd_ret = ismd_event_free(Nero_Events[event]);
        if(ismd_ret != ISMD_SUCCESS)
        {
            printf("failed at ismd_event_free on %d (ismd_evt = %d)\n",event, Nero_Events[event]);
        }
    }
    return (result);
}

Nero_error_t NeroIntelCE4x00VideoDecoder::NeroIntelCE4x00VideoDecoderInvalidateHandles()
{

    NERO_VIDEO_NOTICE ("NeroIntelCE4x00VideoDecoderInvalidateHandles ... \n");
    Nero_error_t     result = NERO_SUCCESS;
    ismd_result_t ismd_ret = ISMD_SUCCESS;
    clock_handle    = ISMD_CLOCK_HANDLE_INVALID;
    viddec_handle   = ISMD_DEV_HANDLE_INVALID;
    vidpproc_handle = ISMD_DEV_HANDLE_INVALID;
    vidrend_handle  = ISMD_DEV_HANDLE_INVALID;

    viddec_input= ISMD_PORT_HANDLE_INVALID;
    viddec_output= ISMD_PORT_HANDLE_INVALID;
    vidpproc_input= ISMD_PORT_HANDLE_INVALID;
    vidpproc_output= ISMD_PORT_HANDLE_INVALID;
    vidrend_input= ISMD_PORT_HANDLE_INVALID;
    VideoDecoderState = NERO_DECODER_LAST;
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

/*************************************************************************
 * public:
**************************************************************************/

NeroIntelCE4x00VideoDecoder::NeroIntelCE4x00VideoDecoder()
{

    NERO_VIDEO_NOTICE ("NeroIntelCE4x00VideoDecoder creator  ... \n");
    Nero_error_t     result = NERO_SUCCESS;
    ismd_result_t ismd_ret;
    internal_clk = true;
    result = NeroIntelCE4x00VideoDecoderInvalidateHandles();
    clock_handle = NeroIntelCE4x00SystemClock::aquire();
}

/** Nero ISMD Video decoder constructor */
NeroIntelCE4x00VideoDecoder::~NeroIntelCE4x00VideoDecoder()
{

    NERO_VIDEO_NOTICE ("NeroIntelCE4x00VideoDecoder killer ... \n");
    Nero_error_t     result = NERO_SUCCESS;
    ismd_result_t ismd_ret;
    clock_handle = ISMD_CLOCK_HANDLE_INVALID;
error:
    result = NeroIntelCE4x00VideoDecoderInvalidateHandles();
}

/** Nero ISMD Video decoder to init and start the decoder  */
Nero_error_t NeroIntelCE4x00VideoDecoder::NeroVideoDecoderInit (Nero_video_codec_t NeroVideoAlgo)
{


    /* Video */
    NERO_VIDEO_NOTICE ("NeroVideoDecoderInit  ... \n");
    Nero_error_t result = NERO_SUCCESS;
    ismd_result_t ismd_ret = ISMD_SUCCESS;
	ismd_codec_type_t vid_type = NeroIntelCE4x00VideoDecoder_NERO2ISMD_codeRemap(NeroVideoAlgo);
    /* init video decoder */

	NERO_VIDEO_NOTICE ("init video decoder  ... \n");



    ismd_ret = ismd_viddec_open(vid_type, &viddec_handle);
    if (ismd_ret!=ISMD_SUCCESS) {
    	result = NERO_ERROR_INTERNAL;
    	NERO_VIDEO_ERROR(" ismd_viddec_open failed \n");
        goto exit;
    }

    ismd_ret = ismd_viddec_set_pts_interpolation_policy(viddec_handle,ISMD_VIDDEC_INTERPOLATE_MISSING_PTS,ISMD_NO_PTS);
    if( ISMD_SUCCESS != ismd_ret ) {
    	NERO_VIDEO_ERROR("ismd_viddec_set_pts_interpolation_policy failed (%d)!", ismd_ret);
        result = NERO_ERROR_INTERNAL;
        goto exit;
    }

    ismd_ret = ismd_viddec_set_max_frames_to_decode(viddec_handle,ISMD_VIDDEC_ALL_FRAMES);
    if(ismd_ret != ISMD_SUCCESS) {
    	NERO_VIDEO_ERROR("ismd_viddec_set_pts_interpolation_policy failed (%d)!", ismd_ret);
        result = NERO_ERROR_INTERNAL;
        goto exit;
    }

    if (ISMD_CODEC_TYPE_MPEG2 == vid_type) {
        ismd_ret = ismd_viddec_set_seq_disp_ext_default_policy(viddec_handle,ISMD_VIDDEC_ISO13818_2);
        if( ISMD_SUCCESS != ismd_ret ) {
        	NERO_VIDEO_ERROR("ismd_viddec_set_seq_disp_ext_default_policy failed (%d)!", ismd_ret);
            result = NERO_ERROR_INTERNAL;
            goto exit;
        }
    }

    ismd_ret = ismd_viddec_get_input_port(viddec_handle,  &viddec_input);
    if (ismd_ret!=ISMD_SUCCESS) {
    	result = NERO_ERROR_INTERNAL;
    	NERO_VIDEO_ERROR(" ismd_viddec_get_input_port failed \n");
        goto exit;
    }

    ismd_ret = ismd_viddec_get_output_port(viddec_handle, &viddec_output);
    if (ismd_ret!=ISMD_SUCCESS) {
    	result = NERO_ERROR_INTERNAL;
    	NERO_VIDEO_ERROR(" ismd_viddec_get_output_port failed \n");
        goto exit;
    }



/* init video proc */
    NERO_VIDEO_NOTICE ("init video Proc  ... \n");
    /* video pproc setup */
    ismd_ret = ismd_vidpproc_open(&vidpproc_handle);
    if (ismd_ret!=ISMD_SUCCESS) {
    	result = NERO_ERROR_INTERNAL;
    	NERO_VIDEO_ERROR(" ismd_vidpproc_open failed \n");
        goto exit;
    }

    ismd_ret = ismd_vidpproc_set_deinterlace_policy(vidpproc_handle, ISMD_VIDPPROC_DI_POLICY_VIDEO);
    if( ISMD_SUCCESS != ismd_ret ) {
    	NERO_VIDEO_ERROR("ismd_vidpproc_set_deinterlace_policy failed (%d)!", ismd_ret);
        result = NERO_ERROR_INTERNAL;
        goto exit;
    }

    ismd_ret =  ismd_vidpproc_set_scaling_policy(vidpproc_handle, ISMD_VIDPPROC_SCALING_POLICY_SCALE_TO_FIT);
    if( ISMD_SUCCESS != ismd_ret ) {
    	NERO_VIDEO_ERROR("ismd_vidpproc_set_scaling_policy failed (%d)!", ismd_ret);
        result = NERO_ERROR_INTERNAL;
        goto exit;
    }

    ismd_ret = ismd_vidpproc_set_dest_params(vidpproc_handle,
               display.tvmode.width,
               display.tvmode.height,
               0x01 ,
               0x01);
    if (ismd_ret!=ISMD_SUCCESS) {
    	result = NERO_ERROR_INTERNAL;
    	NERO_VIDEO_ERROR(" ismd_vidpproc_set_dest_params failed ismd_ret = %d \n", ismd_ret);
        goto exit;
    }

    ismd_ret = ismd_vidpproc_get_input_port(vidpproc_handle, &vidpproc_input);
    if (ismd_ret!=ISMD_SUCCESS) {
    	result = NERO_ERROR_INTERNAL;
    	NERO_VIDEO_ERROR(" ismd_vidpproc_get_input_port failed \n");
        goto exit;
    }

    ismd_ret = ismd_vidpproc_get_output_port(vidpproc_handle, &vidpproc_output);
    if (ismd_ret!=ISMD_SUCCESS) {
    	result = NERO_ERROR_INTERNAL;
    	NERO_VIDEO_ERROR(" ismd_vidpproc_get_output_port failed \n");
        goto exit;
    }

    /* init video Rend */
    NERO_VIDEO_NOTICE ("init video Render  ... \n");
    ismd_ret = ismd_vidrend_open(&vidrend_handle);
    if (ismd_ret!=ISMD_SUCCESS) {
    	result = NERO_ERROR_INTERNAL;
    	NERO_VIDEO_ERROR(" ismd_vidrend_open failed \n");
        goto exit;
    }

    ismd_ret = ismd_vidrend_set_interlaced_display_rate(ISMD_VIDREND_INTERLACED_RATE_50);
    if( ISMD_SUCCESS != ismd_ret ) {
    	NERO_VIDEO_ERROR("ismd_vidrend_set_interlaced_display_rate failed (%d)!", ismd_ret);
        result = NERO_ERROR_INTERNAL;
        goto exit;
    }

    ismd_ret = ismd_vidrend_enable_max_hold_time(vidrend_handle, 10, 1);
    if(ismd_ret != ISMD_SUCCESS) {
    	NERO_VIDEO_ERROR("ismd_vidrend_enable_max_hold_time failed (%d)!", ismd_ret);
        result = NERO_ERROR_INTERNAL;
        goto exit;
    }

    ismd_ret = ismd_vidrend_disable_fixed_frame_rate(vidrend_handle);
    if(ismd_ret != ISMD_SUCCESS) {
    	NERO_VIDEO_ERROR("ismd_vidrend_disable_fixed_frame_rate failed (%d)!", ismd_ret);
        result = NERO_ERROR_INTERNAL;
        goto exit;
    }

    ismd_ret = ismd_vidrend_set_video_plane(vidrend_handle, layer);
    if (ismd_ret!=ISMD_SUCCESS) {
    	result = NERO_ERROR_INTERNAL;
    	NERO_VIDEO_ERROR(" ismd_vidrend_set_video_plane failed \n");
        goto exit;
    }

    ismd_ret = ismd_vidrend_get_input_port(vidrend_handle, &vidrend_input);
    if (ismd_ret!=ISMD_SUCCESS) {
    	result = NERO_ERROR_INTERNAL;
    	NERO_VIDEO_ERROR(" ismd_vidrend_get_input_port failed \n");
        goto exit;
    }



    /***************************************************************************************/
    /************************************** connect pipeline *******************************/
    /***************************************************************************************/

    /* connect pipeline */
    ismd_ret = ismd_port_connect(vidrend_input, vidpproc_output);
    if (ismd_ret!=ISMD_SUCCESS) {
    	result = NERO_ERROR_INTERNAL;
    	NERO_VIDEO_ERROR(" ismd_port_connect failed \n");
        goto exit;
    }

    ismd_ret = ismd_port_connect(vidpproc_input, viddec_output);
    if (ismd_ret!=ISMD_SUCCESS) {
    	result = NERO_ERROR_INTERNAL;
    	NERO_VIDEO_ERROR(" ismd_port_connect failed \n");
        goto exit;
    }
    result = NeroVideoDecoderPause();
    if (NERO_SUCCESS !=result)
    {
    	NERO_VIDEO_ERROR ("NeroVideoDecoderPause fail !!! \n");
        goto exit;
    }
    /* clock setting */

    	ismd_ret = ismd_dev_set_clock(vidrend_handle, clock_handle);
        if (ismd_ret!=ISMD_SUCCESS) {
        	result = NERO_ERROR_INTERNAL;
            NERO_VIDEO_ERROR(" ismd_vidrend_set_timing_mode failed \n");
            goto exit;
        }
    NeroIntelCE4x00VideoDecoder_EventSubscribe();
    //result = NeroVideoDecoderPause();
    if (NERO_SUCCESS !=result)
    {
 	   NERO_VIDEO_ERROR ("NeroVideoDecoderPlay fail !!! \n");
 	   goto exit;
    }

exit:
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

    is_scaling_enabled = GDL_TRUE;
    layer = GDL_PLANE_ID_UPP_B;

    if ((ret_gdl = gdl_init(0))!= GDL_SUCCESS)
    {
    	NERO_VIDEO_ERROR("Failed To initialize GDL gdl_ret = %d \n", ret_gdl);
        result = NERO_ERROR_BAD_PARAMETER;
        goto exit;
    }

    if ((ret_gdl = gdl_get_display_info(GDL_DISPLAY_ID_0, &display))!= GDL_SUCCESS)
    {
    	NERO_VIDEO_ERROR("Failed at gdl_get_display_info gdl_ret = %d \n", ret_gdl);
        result = NERO_ERROR_BAD_PARAMETER;
        goto exit;
    }

    if ((ret_gdl = gdl_plane_reset(layer))!= GDL_SUCCESS)
    {
    	NERO_VIDEO_ERROR("Failed at gdl_plane_reset = %d \n", ret_gdl);
        result = NERO_ERROR_BAD_PARAMETER;
        goto exit;
    }

    if ((ret_gdl = gdl_plane_config_begin(layer))!= GDL_SUCCESS)
    {
    	NERO_VIDEO_ERROR("Failed at gdl_plane_config_begin = %d \n", ret_gdl);
        result = NERO_ERROR_BAD_PARAMETER;
        goto exit;
    }

    rect.origin.x = 0;
    rect.origin.y = 0;
    rect.width    = display.tvmode.width;
    rect.height   = display.tvmode.height;

#if 1
    NERO_VIDEO_NOTICE("graph.rect.origin.x = %d ", rect.origin.x);
    NERO_VIDEO_NOTICE("graph.rect.origin.y = %d ", rect.origin.y);
    NERO_VIDEO_NOTICE("graph.rect.width = %d ", rect.width);
    NERO_VIDEO_NOTICE("graph.rect.height = %d ", rect.height);
#endif

    if ((ret_gdl = gdl_plane_set_attr(GDL_PLANE_DST_RECT, &rect))!= GDL_SUCCESS)
    {
    	NERO_VIDEO_ERROR("Failed at gdl_plane_set_attr = %d \n", ret_gdl);
        result = NERO_ERROR_BAD_PARAMETER;
        goto exit;
    }

    if ((ret_gdl = gdl_plane_set_attr(GDL_PLANE_SCALE, &is_scaling_enabled))!= GDL_SUCCESS)
    {
    	NERO_VIDEO_ERROR("Failed at gdl_plane_set_attr = %d \n", ret_gdl);
        result = NERO_ERROR_BAD_PARAMETER;
        goto exit;
    }

    if ((ret_gdl = gdl_plane_config_end(GDL_FALSE))!= GDL_SUCCESS)
    {
    	NERO_VIDEO_ERROR("Failed at gdl_plane_config_end = %d \n", ret_gdl);
        result = NERO_ERROR_BAD_PARAMETER;
        goto exit;
    }

exit:
    return (result);
}

/** Nero ISMD Video decoder to on fly reconfigure Video decoder  */

Nero_error_t  NeroIntelCE4x00VideoDecoder::NeroVideoDecoderResizeLayer(size_t w,size_t h,size_t x,size_t y)
{
    NERO_VIDEO_NOTICE ("NeroVideoDecoderResizeLayer  ... \n");
    Nero_error_t result = NERO_SUCCESS;
    ismd_result_t ismd_ret= ISMD_SUCCESS;
    gdl_ret_t ret_gdl = GDL_SUCCESS;


#if 1
    NERO_VIDEO_NOTICE("display.tvmode.width = %d \n",display.tvmode.width);
    NERO_VIDEO_NOTICE("display.tvmode.height = %d \n",display.tvmode.height);
    NERO_VIDEO_NOTICE("rect.x = %d \n",x);
    NERO_VIDEO_NOTICE("rect.y = %d \n",y);
    NERO_VIDEO_NOTICE("rect.width = %d \n",w);
    NERO_VIDEO_NOTICE("rect.height = %d \n", h);
    NERO_VIDEO_NOTICE("alpha = %d \n", alpha);
#endif
    if ((x + w > display.tvmode.width)||
        (y + h > display.tvmode.height))
    {
    	NERO_VIDEO_ERROR("The requested size is larger than screen capabilities");
        result = NERO_ERROR_BAD_PARAMETER;
        goto exit;
    }

    rect.origin.x = x;
    rect.origin.y = y;
    rect.width    = w;
    rect.height   = h;
    //alpha         = p.alpha;

    if ((ismd_ret = ismd_vidpproc_set_dest_params(vidpproc_handle, rect.width, rect.height, 1, 1))!= ISMD_SUCCESS)
    {
    	NERO_VIDEO_ERROR("failed at ismd_vidpproc_set_dest_params = %08x \n",ismd_ret);
    	result = NERO_ERROR_INTERNAL;
    }

    if ((ret_gdl = gdl_plane_config_begin(layer))!= GDL_SUCCESS)
    {
    	NERO_VIDEO_ERROR("Failed at gdl_plane_config_begin = %d \n", ret_gdl);
        result = NERO_ERROR_INTERNAL;
        goto exit;
    }

    if ((ret_gdl = gdl_plane_set_attr(GDL_PLANE_DST_RECT, &rect))!= GDL_SUCCESS)
    {
    	NERO_VIDEO_ERROR("Failed at gdl_plane_set_attr = %d \n", ret_gdl);
        result = NERO_ERROR_INTERNAL;
        goto exit;
    }

    if ((ret_gdl = gdl_plane_config_end(GDL_FALSE))!= GDL_SUCCESS)
    {
    	NERO_VIDEO_ERROR("Failed at gdl_plane_config_end = %d \n", ret_gdl);
        result = NERO_ERROR_INTERNAL;
        goto exit;
    }

exit:
    return (result);
}


Nero_error_t NeroIntelCE4x00VideoDecoder::NeroVideoDecoderUnInit()
{
    NERO_VIDEO_NOTICE ("NeroVideoDecoderUnInit  ... \n");
    Nero_error_t result = NERO_SUCCESS;
    ismd_result_t ismd_ret= ISMD_SUCCESS;
    /*_TRY(ismd_ret,pause_video_pipeline,());*/
    NeroIntelCE4x00VideoDecoder_EventUnSubscribe();
    result = NeroVideoDecoderPause();
    if (NERO_SUCCESS !=result)
    {
    	NERO_VIDEO_ERROR ("NeroVideoDecoderPause fail !!! \n");
        goto error;
    }
    result = NeroVideoDecoderFlush();
    if (NERO_SUCCESS !=result)
    {
        NERO_VIDEO_ERROR ("NeroVideoDecoderFlush fail !!! \n");
   	    goto error;
    }


    result = NeroVideoDecoderStop();
    if (NERO_SUCCESS !=result)
    {
        NERO_VIDEO_ERROR ("NeroVideoDecoderStop fail !!! \n");
   	    goto error;
    }

     /*_TRY(ismd_ret,disconnect_video_pipeline,());*/
    /***************************************************************************************/
    /*********************************** Disconnects ports *********************************/
    /***************************************************************************************/

    ismd_ret = ismd_port_disconnect(viddec_input);
    if (ismd_ret != ISMD_SUCCESS) {
    	NERO_VIDEO_WARNING("Failed at ismd_port_disconnect ismd_ret = %d \n", ismd_ret);
        result = NERO_ERROR_INTERNAL;
    }

    ismd_ret = ismd_port_detach(viddec_input);
    if (ismd_ret != ISMD_SUCCESS) {
    	NERO_VIDEO_WARNING("Failed at ismd_port_detach ismd_ret = %d \n", ismd_ret);
        result = NERO_ERROR_INTERNAL;
    }

    ismd_ret = ismd_port_disconnect(viddec_output);
    if (ismd_ret != ISMD_SUCCESS) {
    	NERO_VIDEO_WARNING("Failed at ismd_port_disconnect ismd_ret = %d \n", ismd_ret);
        result = NERO_ERROR_INTERNAL;
    }

    ismd_ret = ismd_port_detach(viddec_output);
    if (ismd_ret != ISMD_SUCCESS) {
    	NERO_VIDEO_WARNING("Failed at ismd_port_detach ismd_ret = %d \n", ismd_ret);
        result = NERO_ERROR_INTERNAL;
    }

    ismd_ret = ismd_port_disconnect(vidpproc_input);
    if (ismd_ret != ISMD_SUCCESS) {
    	NERO_VIDEO_WARNING("Failed at ismd_port_disconnect ismd_ret = %d \n", ismd_ret);
        result = NERO_ERROR_INTERNAL;
    }

    ismd_ret = ismd_port_detach(vidpproc_input);
    if (ismd_ret != ISMD_SUCCESS) {
    	NERO_VIDEO_WARNING("Failed at ismd_port_detach ismd_ret = %d \n", ismd_ret);
        result = NERO_ERROR_INTERNAL;
    }

    ismd_ret = ismd_port_disconnect(vidpproc_output);
    if (ismd_ret != ISMD_SUCCESS) {
    	NERO_VIDEO_WARNING("Failed at ismd_port_disconnect ismd_ret = %d \n", ismd_ret);
        result = NERO_ERROR_INTERNAL;
    }

    ismd_ret = ismd_port_detach(vidpproc_output);
    if (ismd_ret != ISMD_SUCCESS) {
    	NERO_VIDEO_WARNING("Failed at ismd_port_detach ismd_ret = %d \n", ismd_ret);
        result = NERO_ERROR_INTERNAL;
    }

    ismd_ret = ismd_port_disconnect(vidrend_input);
    if (ismd_ret != ISMD_SUCCESS) {
    	NERO_VIDEO_WARNING("Failed at ismd_port_disconnect ismd_ret = %d \n", ismd_ret);
        result = NERO_ERROR_INTERNAL;
    }

    ismd_ret = ismd_port_detach(vidrend_input);
    if (ismd_ret != ISMD_SUCCESS) {
    	NERO_VIDEO_WARNING("Failed at ismd_port_detach ismd_ret = %d \n", ismd_ret);
        result = NERO_ERROR_INTERNAL;
    }

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
        NERO_VIDEO_ERROR ("NeroVideoDecoderPlay fail !!! \n");
        goto error;
    }
    VideoDecoderState = NERO_DECODER_LAST;
error:
    return(result);

}

/** Nero ISMD Video decoder fluch pipeline  */

Nero_error_t NeroIntelCE4x00VideoDecoder::NeroVideoDecoderClose()
{

    NERO_VIDEO_NOTICE ("NeroVideoDecoderClose  ... \n");
    Nero_error_t result = NERO_SUCCESS;
    ismd_result_t ismd_ret = ISMD_SUCCESS;
    /***************************************************************************************/
    /*********************************** Close the devices *********************************/
    /***************************************************************************************/
    ismd_ret = ismd_dev_close(viddec_handle);
    if (ismd_ret != ISMD_SUCCESS) {
    	NERO_VIDEO_WARNING("Failed at ismd_dev_close ismd_ret = %d \n", ismd_ret);
        result = NERO_ERROR_INTERNAL;
    }

    ismd_ret = ismd_dev_close(vidpproc_handle);
    if (ismd_ret != ISMD_SUCCESS) {
    	NERO_VIDEO_WARNING("Failed at ismd_dev_close ismd_ret = %d \n", ismd_ret);
        result = NERO_ERROR_INTERNAL;
    }
    ismd_ret = ismd_dev_close(vidrend_handle);
    if (ismd_ret != ISMD_SUCCESS) {
    	NERO_VIDEO_WARNING("Failed at ismd_dev_close ismd_ret = %d \n", ismd_ret);
        result = NERO_ERROR_INTERNAL;
    }
    VideoDecoderState = NERO_DECODER_STOP;
exit:
     return (result);
}

/** Nero ISMD Video decoder fluch pipeline  */

Nero_error_t NeroIntelCE4x00VideoDecoder::NeroVideoDecoderFlush()
{
    NERO_VIDEO_NOTICE ("NeroVideoDecoderFlush  ... \n");
    Nero_error_t result = NERO_SUCCESS;
    ismd_result_t ismd_ret;

    ismd_ret = ismd_dev_flush(viddec_handle);
    if(ismd_ret != ISMD_SUCCESS) {
    	NERO_VIDEO_ERROR("failed at ismd_dev_flush, viddec_handle\n");
        result= NERO_ERROR_INTERNAL;
        goto exit;
    }

    ismd_ret = ismd_dev_flush(vidpproc_handle);
    if(ismd_ret != ISMD_SUCCESS) {
    	NERO_VIDEO_ERROR("failed at ismd_dev_flush, vidpproc_handle\n");
        result= NERO_ERROR_INTERNAL;
        goto exit;
    }

    ismd_ret = ismd_dev_flush(vidrend_handle);
    if(ismd_ret != ISMD_SUCCESS) {
    	NERO_VIDEO_ERROR("failed at ismd_dev_flush, vidrend_handle\n");
        result= NERO_ERROR_INTERNAL;
        goto exit;
    }

exit:
    return (result);

}

/** Nero ISMD Video decoder fluch pipeline  */
Nero_error_t NeroIntelCE4x00VideoDecoder::NeroVideoDecoderStop()
{
    NERO_VIDEO_NOTICE ("NeroVideoDecoderStop  ... \n");
    Nero_error_t result = NERO_SUCCESS;
    ismd_result_t ismd_ret;

    if(VideoDecoderState != NERO_DECODER_STOP)
    {
        ismd_ret = ismd_dev_set_state(viddec_handle, ISMD_DEV_STATE_STOP);
        if(ismd_ret!=ISMD_SUCCESS){
    	    NERO_VIDEO_WARNING("failed at ismd_dev_set_state ismd_ret = %08x \n",ismd_ret);
            result = NERO_ERROR_INTERNAL;
        }

        ismd_ret = ismd_dev_set_state(vidpproc_handle, ISMD_DEV_STATE_STOP);
        if(ismd_ret!=ISMD_SUCCESS){
    	    NERO_VIDEO_WARNING("failed at ismd_dev_set_state ismd_ret = %08x \n",ismd_ret);
            result = NERO_ERROR_INTERNAL;
        }

        ismd_ret = ismd_dev_set_state(vidrend_handle, ISMD_DEV_STATE_STOP);
        if(ismd_ret!=ISMD_SUCCESS){
    	    NERO_VIDEO_WARNING("failed at ismd_dev_set_state ismd_ret = %08x \n",ismd_ret);
            result = NERO_ERROR_INTERNAL;
        }
        VideoDecoderState = NERO_DECODER_STOP;
    }
error:
    return (result);
}

/** Nero ISMD Video decoder pause pipeline  */

Nero_error_t NeroIntelCE4x00VideoDecoder::NeroVideoDecoderPause()
{
    NERO_VIDEO_NOTICE ("NeroVideoDecoderPause  ... \n");
    Nero_error_t result = NERO_SUCCESS;
    ismd_result_t ismd_ret;
    ismd_time_t pause_time = NeroIntelCE4x00SystemClock::GetTime();
    if ((VideoDecoderState != NERO_DECODER_PAUSE)/* && (VideoDecoderState != NERO_DECODER_LAST)*/)
    {
        result = NeroIntelCE4x00SystemClock::SetPauseTime(pause_time);

        if (NERO_SUCCESS!=result)
        {
        	NERO_VIDEO_WARNING("SetPauseTime fail (%d)  \n",result);
        	goto error;
        }
        ismd_ret = ismd_dev_set_state(viddec_handle, ISMD_DEV_STATE_PAUSE);
        if(ismd_ret!=ISMD_SUCCESS){
    	    NERO_VIDEO_WARNING("failed at ismd_dev_set_state ismd_ret = %08x \n",ismd_ret);
            result = NERO_ERROR_INTERNAL;
        }

        ismd_ret = ismd_dev_set_state(vidpproc_handle, ISMD_DEV_STATE_PAUSE);
        if(ismd_ret!=ISMD_SUCCESS){
    	    NERO_VIDEO_WARNING("failed at ismd_dev_set_state ismd_ret = %08x \n",ismd_ret);
            result = NERO_ERROR_INTERNAL;
        }

        ismd_ret = ismd_dev_set_state(vidrend_handle, ISMD_DEV_STATE_PAUSE);
        if(ismd_ret!=ISMD_SUCCESS){
    	    NERO_VIDEO_WARNING("failed at ismd_dev_set_state ismd_ret = %08x \n",ismd_ret);
            result = NERO_ERROR_INTERNAL;
        }

        VideoDecoderState = NERO_DECODER_PAUSE;
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
    ismd_time_t   pause_duration = 0;
    ismd_time_t   curr_time = 0;
    ismd_time_t   pause_time = 0;
    ismd_time_t   base_time = 0;
    Nero_stc_type_t stc_type = NERO_STC_FREERUN;
    if ((VideoDecoderState != NERO_DECODER_PLAY) /*&&(VideoDecoderState != NERO_DECODER_LAST)*/)
    {
        ismd_ret = ismd_dev_set_state(viddec_handle, ISMD_DEV_STATE_PLAY);
        if(ismd_ret != ISMD_SUCCESS) {
    	    NERO_VIDEO_WARNING("ismd_dev_set_state, viddec_dev PLAY failed (%d)! \n", ismd_ret);
            result = NERO_ERROR_INTERNAL;
        }

        ismd_ret = ismd_dev_set_state(vidpproc_handle, ISMD_DEV_STATE_PLAY);
        if(ismd_ret != ISMD_SUCCESS) {
    	    NERO_VIDEO_WARNING("ismd_dev_set_state, vidpproc_dev PLAY failed (%d)! \n", ismd_ret);
            result = NERO_ERROR_INTERNAL;
        }
        /* calculate the new base time */
        curr_time  = NeroIntelCE4x00SystemClock::GetTime();
        pause_time = NeroIntelCE4x00SystemClock::GetPauseTime();
        base_time = NeroIntelCE4x00SystemClock::GetBaseTime();
        pause_duration = curr_time - pause_time;
        base_time += pause_duration;
        NeroIntelCE4x00SystemClock::SetBaseTime(base_time);
        /* Set the new base time */
        ismd_ret = ismd_dev_set_stream_base_time (vidrend_handle, base_time);
        if ( ISMD_SUCCESS != ismd_ret ) {
        	NERO_VIDEO_ERROR("ismd_dev_set_stream_base_time failed (%d)", ismd_ret);
        }

        if(ismd_ret != ISMD_SUCCESS) {
    	    NERO_VIDEO_WARNING("ismd_dev_set_stream_base_time failed (%d)! \n", ismd_ret);
            result = NERO_ERROR_INTERNAL;
            goto error;
        }

       ismd_ret = ismd_dev_set_state(vidrend_handle, ISMD_DEV_STATE_PLAY);

       result = NeroVideoDecoderFlush();
       if (NERO_SUCCESS !=result)
       {
           NERO_VIDEO_ERROR ("NeroVideoDecoderFlush fail !!! \n");
  	       goto error;
       }
       VideoDecoderState = NERO_DECODER_PLAY;
    }
error:
    return (result);

}

Nero_error_t   NeroIntelCE4x00VideoDecoder::NeroVideoDecoderFeed (uint8_t* payload, size_t payload_size, uint64_t nrd_pts, bool discontinuity)
{
	/*********************************/
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
        while (ismd_port_write(viddec_input, bufin_handle) != ISMD_SUCCESS) {
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
    }
    NERO_VIDEO_ERROR ("video_pts = %d  ... \n",video_pts);
error:
    return((uint64_t)video_pts);
}
/* for testing */
int NeroIntelCE4x00VideoDecoder::NeroVideoDecoderGetport()
{
    NERO_VIDEO_NOTICE ("NeroVideoDecoderGetport  ... \n");
    return ((int)viddec_input);
}
Nero_error_t   NeroIntelCE4x00VideoDecoder::NeroVideoDecoderEventWait(NeroEvents_t *event)
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
                	printf ("V- NERO_EVENT_FRAME_FLIPPED .. \n");
                    event->pts = NeroVideoDecoderGetLastPts();
                    event->header = (Nero_Event_Liste_t) evt;
                    break;
                }
                case NERO_EVENT_UNDERFLOW:
                {
                	printf ("V- NERO_EVENT_UNDERFLOW .. \n");
                    event->header = (Nero_Event_Liste_t) evt;
                    break;
                }
                case NERO_EVENT_UNDERFLOWRECOVRED:
                {
                	printf ("V- NERO_EVENT_UNDERFLOWRECOVRED .. \n");
                    event->header = (Nero_Event_Liste_t) evt;
                    break;
                }
                case NERO_EVENT_PTS_VALUE_EARLY        :
                case NERO_EVENT_PTS_VALUE_LATE         :
                case NERO_EVENT_PTS_PTS_VALUE_RECOVERED:
                case NERO_EVENT_LAST:
                default:
                    printf("V- Unknown event %d\n", evt);
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
