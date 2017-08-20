/*
 * NeroIntelCE4x00STC.cpp
 *
 *  Created on: Aug 12, 2017
 *      Author: g360476
 */

#include "NeroIntelCE4x00STC.h"
/*************************************************************************
 * private:
**************************************************************************/

Nero_error_t NeroIntelCE4x00STC::NeroIntelCE4x00STCInvalidateHandles()
{

    NERO_STC_NOTICE ("NeroIntelCE4x00STCInvalidateHandles ... \n");
    Nero_error_t     result = NERO_SUCCESS;
    clock_handle            = ISMD_DEV_HANDLE_INVALID;

    return(result);

}

/*************************************************************************
 * public:
**************************************************************************/
/** Nero ISMD stc clock constructor */
NeroIntelCE4x00STC::NeroIntelCE4x00STC()
{

    NERO_AUDIO_NOTICE ("NeroIntelCE4x00STC creator  ... \n");
    Nero_error_t     result = NERO_SUCCESS;
    ismd_result_t ismd_ret = ISMD_SUCCESS;
    result = NeroIntelCE4x00STCInvalidateHandles();
    basetime = 0x00;
    ismd_ret = ismd_clock_alloc_typed(ISMD_CLOCK_CLASS_SLAVE,ISMD_CLOCK_DOMAIN_TYPE_VIDEO,&clock_handle);
    if(ISMD_SUCCESS != ismd_ret)
    {
		result = NERO_ERROR_INTERNAL;
	}
    
}

NeroIntelCE4x00STC::NeroIntelCE4x00STC(uint8_t type)
{

    NERO_AUDIO_NOTICE ("NeroIntelCE4x00STC creator  ... \n");
    Nero_error_t     result = NERO_SUCCESS;
    ismd_result_t ismd_ret = ISMD_SUCCESS;
    basetime = 0x00;
    result = NeroIntelCE4x00STCInvalidateHandles();
    ismd_ret = ismd_clock_alloc_typed(ISMD_CLOCK_CLASS_SLAVE,ISMD_CLOCK_DOMAIN_TYPE_VIDEO,&clock_handle);
    if(ismd_ret != ISMD_SUCCESS){
    	NERO_AUDIO_ERROR("ismd_clock_alloc_typed Failed ");
        result = NERO_ERROR_INTERNAL;
    }

    /* sets the current time to 0 */
    basetime = 0x00;

    ismd_ret = ismd_clock_set_time(clock_handle, basetime);
    if(ismd_ret != ISMD_SUCCESS){
    	NERO_AUDIO_ERROR("ismd_clock_alloc_typed Failed ");
        result = NERO_ERROR_INTERNAL;
    }
/*
    ismd_ret = ismd_clock_alloc(ISMD_CLOCK_TYPE_FIXED, &clock_handle);
    if(ISMD_SUCCESS != ismd_ret)
    {
	    result = NERO_ERROR_INTERNAL;
	}
    if(type = NERO_AV_PLAYBACk_CLK)
    {
        ismd_ret = ismd_clock_make_primary(clock_handle);
        if(ISMD_SUCCESS != ismd_ret)
        {
		    result = NERO_ERROR_INTERNAL;
	    }
        ismd_ret = ismd_clock_set_vsync_pipe(clock_handle, 0);
        if(ISMD_SUCCESS != ismd_ret)
        {
		    result = NERO_ERROR_INTERNAL;
	    }
	}
*/

}

NeroIntelCE4x00STC::~NeroIntelCE4x00STC()
{
    NERO_AUDIO_NOTICE ("NeroIntelCE4x00STC killer  ... \n");
    Nero_error_t     result = NERO_SUCCESS;
    ismd_result_t ismd_ret = ISMD_SUCCESS;
	_TRY(ismd_ret,result, ismd_clock_free,(clock_handle));

error:
    result = NeroIntelCE4x00STCInvalidateHandles();
}

Nero_error_t NeroIntelCE4x00STC::NeroSTCSetPlayBack()
{
    NERO_AUDIO_NOTICE ("NeroSTCSetPlayBack ... \n");
    Nero_error_t     result = NERO_SUCCESS;
    ismd_result_t ismd_ret = ISMD_SUCCESS;
    _TRY(ismd_ret,result, ismd_clock_make_primary,(clock_handle));
    _TRY(ismd_ret,result, ismd_clock_set_vsync_pipe,(clock_handle, 0));
error:
    return(result);
}

nero_clock_handle_t NeroIntelCE4x00STC::NeroSTCGetClock()
{
    NERO_AUDIO_NOTICE ("NeroSTCSetPlayBack ... \n");
    return((nero_clock_handle_t)clock_handle);

}

Nero_error_t NeroIntelCE4x00STC::NeroSTCSetBaseTime(uint64_t time)
{
    NERO_AUDIO_NOTICE ("NeroSTCSetBaseTime ... %d \n", time);
    basetime = time;
    return(NERO_SUCCESS);

}

uint64_t NeroIntelCE4x00STC::NeroSTCGetBaseTime()
{
    NERO_AUDIO_NOTICE ("NeroSTCGetBaseTime ... (%d)\n",basetime);
    return((uint64_t)basetime);

}

Nero_stc_type_t NeroIntelCE4x00STC::NeroSTCGetType()
{
    NERO_AUDIO_NOTICE ("NeroSTCGetType ... \n");
    return((Nero_stc_type_t)Nero_stc_type);

}

Nero_error_t NeroIntelCE4x00STC::NeroSTCSetType(Nero_stc_type_t stc_type)
{
    NERO_AUDIO_NOTICE ("NeroSTCSetType ... \n");
    Nero_stc_type = stc_type;
}
