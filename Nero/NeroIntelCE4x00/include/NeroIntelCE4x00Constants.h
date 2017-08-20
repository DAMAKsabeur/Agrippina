extern "C"
{
#include "ismd_global_defs.h"
}
/* audio traces debug */
#define NERO_AUDIO_DEBUG printf
#define NERO_AUDIO_NOTICE printf
#define NERO_AUDIO_WARNING printf
#define NERO_AUDIO_ERROR printf
/* */
/* video traces debug */
#define NERO_VIDEO_DEBUG printf
#define NERO_VIDEO_NOTICE printf
#define NERO_VIDEO_WARNING printf
#define NERO_VIDEO_ERROR printf

/* stc traces debug */
#define NERO_STC_DEBUG printf
#define NERO_STC_NOTICE printf
#define NERO_STC_WARNING printf
#define NERO_STC_ERROR printf

#define DISPLAY_WIDTH 1920
#define DISPLAY_HEIGHT 1080
typedef ismd_time_t nero_time_t ;
typedef ismd_clock_t nero_clock_handle_t;
#define _TRY(ret,result,api,params) do { \
    ret = api params; \
    if(ret != ISMD_SUCCESS)\
    {\
        printf("%s:%d:%s: %s failed with %u\n",__FILE__,__LINE__,__FUNCTION__,#api,ret);\
        result = NERO_ERROR_INTERNAL; \
        goto error;\
    }\
} while(0)
