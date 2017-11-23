#include "NeroIntelCE4x00SystemClock.h"

ismd_clock_t               Nero_clock = ISMD_CLOCK_HANDLE_INVALID;
ismd_time_t pause_time;
ismd_time_t base_time;
static bool isInit = false;
using namespace std;
Nero_error_t NeroIntelCE4x00SystemClock::init()
{
    NERO_AUDIO_NOTICE ("NeroIntelCE4x00SystemClock creator  ... \n");
    Nero_error_t     result = NERO_SUCCESS;
    ismd_clock_t               clock;
    ismd_result_t ismd_ret = ISMD_SUCCESS;
    //ismd_ret =ismd_clock_alloc(ISMD_CLOCK_TYPE_FIXED, &clock);
    ismd_ret = ismd_clock_alloc_typed(ISMD_CLOCK_CLASS_MASTER,ISMD_CLOCK_DOMAIN_TYPE_AV,&clock);
    if(ISMD_SUCCESS != ismd_ret)
    {
		result = NERO_ERROR_INTERNAL;
		goto exit;
	}
    ismd_ret =ismd_clock_make_primary(clock);
    if(ISMD_SUCCESS != ismd_ret)
    {
		result = NERO_ERROR_INTERNAL;
		goto exit;
	}
    ismd_ret = ismd_clock_set_vsync_pipe(clock, 0);
    if(ISMD_SUCCESS != ismd_ret)
    {
		result = NERO_ERROR_INTERNAL;
		goto exit;
	}
    pause_time = 0x00;
    base_time  = 0x00;
    isInit = true;
    Nero_clock = clock;
exit:
    return (result);
}

Nero_error_t NeroIntelCE4x00SystemClock::fini()
{
    NERO_AUDIO_NOTICE ("NeroIntelCE4x00SystemClock killer  ... \n");
    Nero_error_t     result = NERO_SUCCESS;
    ismd_clock_t               clock;
    ismd_result_t ismd_ret = ISMD_SUCCESS;
	ismd_ret = ismd_clock_free(Nero_clock);
    if(ISMD_SUCCESS != ismd_ret)
    {
		result = NERO_ERROR_INTERNAL;
		goto exit;
	}
    isInit = false;
    Nero_clock = ISMD_CLOCK_HANDLE_INVALID;
exit:
    return(result);
}

ismd_time_t NeroIntelCE4x00SystemClock::GetTime()
{
    ismd_result_t ret_ismd = ISMD_SUCCESS;
    ismd_time_t curr_time = 0;

    ret_ismd = ismd_clock_get_time(Nero_clock, &curr_time);
    if (ret_ismd != ISMD_SUCCESS) {
        printf("ismd_clock_get_time Failed ");
    }

    return (curr_time);
}

ismd_time_t NeroIntelCE4x00SystemClock::GetPauseTime()
{
    NERO_AUDIO_NOTICE ("NeroIntelCE4x00SystemClock GetPauseTime  ... \n");
    Nero_error_t     result = NERO_SUCCESS;
    return (pause_time);
}
ismd_time_t NeroIntelCE4x00SystemClock::GetBaseTime()
{
    NERO_AUDIO_NOTICE ("NeroIntelCE4x00SystemClock GetBaseTime  ... \n");
    Nero_error_t     result = NERO_SUCCESS;
    return (base_time);
}

Nero_error_t NeroIntelCE4x00SystemClock::SetPauseTime(ismd_time_t time)
{
    NERO_AUDIO_NOTICE ("NeroIntelCE4x00SystemClock SetPauseTime  ... \n");
    Nero_error_t     result = NERO_SUCCESS;
    pause_time = time;
    return (NERO_SUCCESS);
}
Nero_error_t NeroIntelCE4x00SystemClock::SetBaseTime(ismd_time_t time)
{
    NERO_AUDIO_NOTICE ("NeroIntelCE4x00SystemClock GetTime  ... \n");
    Nero_error_t     result = NERO_SUCCESS;
    base_time = time;
    return (NERO_SUCCESS);
}
ismd_clock_t NeroIntelCE4x00SystemClock::aquire()
{
    NERO_AUDIO_NOTICE ("NeroIntelCE4x00SystemClock aquire  ... \n");
    Nero_error_t     result = NERO_SUCCESS;
    ismd_clock_t               clock = ISMD_CLOCK_HANDLE_INVALID;
    ismd_result_t ismd_ret = ISMD_SUCCESS;
    if ((isInit == true) && (Nero_clock != ISMD_CLOCK_HANDLE_INVALID))
    {
    	clock = Nero_clock;
    }

    return (clock);
}
