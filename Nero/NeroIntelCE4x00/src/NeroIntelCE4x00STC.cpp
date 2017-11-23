/*
 * NeroIntelCE4x00STC.cpp
 *
 *  Created on: Aug 12, 2017
 *      Author: g360476
 */

#include "NeroIntelCE4x00STC.h"
using namespace std;


/*************************************************************************
 * STC thread :
**************************************************************************/
/*void * STC_DecoderTask(void * args)
{
	NeroIntelCE4x00STC* stc_handle = (NeroIntelCE4x00STC*)args;
	NeroAudioDecoder* AudioDecoder_ptr=  stc_handle->GetAudioDecoder();
	NeroVideoDecoder* VideoDecoder_ptr=  stc_handle->GetVideoDecoder();
	Nero_stc_type_t SyncMode = stc_handle->GetSyncMode();
	NeroBlockingQueue * VideoESQueue;
	NeroBlockingQueue * AudioESQueue;
	AudioESQueue = &AudioDecoder_ptr->AudioESQueue;
	VideoESQueue = &VideoDecoder_ptr->VideoESQueue;
	ES_t Audio_ES_inj;
	ES_t Video_ES_inj;
    while (true)
    {*/
//    	if (!VideoESQueue->empty())
//    	{
//    	Video_ES_inj= VideoESQueue->front();
//    	if (!AudioESQueue->empty())
//    	{
//    	    Audio_ES_inj= AudioESQueue->front();
//    	    if ((Video_ES_inj.pts - Audio_ES_inj.pts) > 500) /* Audio is too late */
//		    {
//                printf("Audio is too late \n");
//    		    do	{
//            	    free(Audio_ES_inj.buffer);
//            	    Audio_ES_inj= AudioESQueue->pop();
//            	    Audio_ES_inj= AudioESQueue->front();
//
//    		        } while((Video_ES_inj.pts - Audio_ES_inj.pts) <= 500);
//    	    	AudioDecoder_ptr->Feed((uint8_t*)Audio_ES_inj.buffer,Audio_ES_inj.size,Audio_ES_inj.pts,Audio_ES_inj.discontinuity);
//    	    	VideoDecoder_ptr->Feed((uint8_t*)Video_ES_inj.buffer,Video_ES_inj.size,Video_ES_inj.pts,Video_ES_inj.discontinuity);
//    	    	stc_handle->last_syc_pts = Video_ES_inj.pts;
//    		    free(Video_ES_inj.buffer);
//    		    free(Audio_ES_inj.buffer);
//    		    AudioESQueue->pop();
//    		    VideoESQueue->pop();
//		    }
//    	    else if ((Video_ES_inj.pts - Audio_ES_inj.pts) <= -200) /* Audio is too early  */
//    	    {
//    		    printf("Audio is too early \n");
//                 /* wait until the pts become mature */
//        	    VideoDecoder_ptr->Feed((uint8_t*)Video_ES_inj.buffer,Video_ES_inj.size,Video_ES_inj.pts,Video_ES_inj.discontinuity);
//    	        free(Video_ES_inj.buffer);
//    	        VideoESQueue->pop();
//    		    continue;
//    	    }
//    	    else
//    	    {
//    		    printf("Audio is on time \n");
//    	    	AudioDecoder_ptr->Feed((uint8_t*)Audio_ES_inj.buffer,Audio_ES_inj.size,Audio_ES_inj.pts,Audio_ES_inj.discontinuity);
//    	    	VideoDecoder_ptr->Feed((uint8_t*)Video_ES_inj.buffer,Video_ES_inj.size,Video_ES_inj.pts,Video_ES_inj.discontinuity);
//    	    	stc_handle->last_syc_pts = Video_ES_inj.pts;
//    		    free(Video_ES_inj.buffer);
//    		    free(Audio_ES_inj.buffer);
//    		    VideoESQueue->pop();
//    		    AudioESQueue->pop();
//    	    }
//    	}else
//    	{
//        	VideoDecoder_ptr->Feed((uint8_t*)Video_ES_inj.buffer,Video_ES_inj.size,Video_ES_inj.pts,Video_ES_inj.discontinuity);
//        	stc_handle->last_syc_pts = Video_ES_inj.pts;
//    	    free(Video_ES_inj.buffer);
//    	    VideoESQueue->pop();
//    	}
//        }
//    	else
//    	{
//    		/*do
//    		{
//    		Audio_ES_inj= AudioESQueue->front();
//		    free(Audio_ES_inj.buffer);
//		    AudioESQueue->pop();
//    		}while (AudioESQueue->empty());*/
//    	}
    /*	if (!AudioESQueue->empty())
    	{
        	Audio_ES_inj= AudioESQueue->front();
        	AudioDecoder_ptr->Feed((uint8_t*)Audio_ES_inj.buffer,Audio_ES_inj.size,Audio_ES_inj.pts,Audio_ES_inj.discontinuity);
        	free(Audio_ES_inj.buffer);
       	    AudioESQueue->pop();
    	}
    	if (!VideoESQueue->empty())
    	{
        	Video_ES_inj= VideoESQueue->front();
        	VideoDecoder_ptr->Feed((uint8_t*)Video_ES_inj.buffer,Video_ES_inj.size,Video_ES_inj.pts,Video_ES_inj.discontinuity);
        	free(Video_ES_inj.buffer);

        	VideoESQueue->pop();
        	stc_handle->last_syc_pts = Video_ES_inj.pts;
    	}

        os_sleep(50);
    }

}*/
/*************************************************************************
 * private:
**************************************************************************/


/*************************************************************************
 * public:
**************************************************************************/
/** Nero ISMD stc clock constructor */
NeroIntelCE4x00STC::NeroIntelCE4x00STC()
{

    NERO_AUDIO_NOTICE ("NeroIntelCE4x00STC creator  ... \n");
    m_NeroAudioDecoder_ptr = NULL;
    m_NeroVideoDecoder_ptr = NULL;
    m_SyncMode = NERO_STC_FREERUN;
    last_syc_pts = 0x00;

}

NeroIntelCE4x00STC::NeroIntelCE4x00STC(Nero_stc_type_t SyncMode)
{

    NERO_AUDIO_NOTICE ("NeroIntelCE4x00STC creator  ... \n");
    m_NeroAudioDecoder_ptr = NULL;
    m_NeroVideoDecoder_ptr = NULL;
    m_SyncMode = SyncMode;
    last_syc_pts = 0x00;
}

NeroIntelCE4x00STC::~NeroIntelCE4x00STC()
{
    NERO_AUDIO_NOTICE ("NeroIntelCE4x00STC killer  ... \n");
    m_NeroAudioDecoder_ptr = NULL;
    m_NeroVideoDecoder_ptr = NULL;
    m_SyncMode = NERO_STC_FREERUN;

}

Nero_error_t NeroIntelCE4x00STC::ConncectSources(NeroAudioDecoder* NeroAudioDecoder_ptr, NeroVideoDecoder* NeroVideoDecoder_ptr)
{
    m_NeroAudioDecoder_ptr = NeroAudioDecoder_ptr;
    m_NeroVideoDecoder_ptr = NeroVideoDecoder_ptr;

}
Nero_error_t NeroIntelCE4x00STC::DisconncectSources()
{
    m_NeroAudioDecoder_ptr = NULL;
    m_NeroVideoDecoder_ptr = NULL;
    m_SyncMode = NERO_STC_FREERUN;
}

NeroAudioDecoder* NeroIntelCE4x00STC::GetAudioDecoder ()
{
	return (m_NeroAudioDecoder_ptr);
}

NeroVideoDecoder* NeroIntelCE4x00STC::GetVideoDecoder ()
{
	return (m_NeroVideoDecoder_ptr);
}

Nero_error_t NeroIntelCE4x00STC::SetSyncMode (Nero_stc_type_t SyncMode)
{
	m_SyncMode = SyncMode;
}

Nero_stc_type_t NeroIntelCE4x00STC::GetSyncMode ()
{
    return (m_SyncMode);
}
uint64_t NeroIntelCE4x00STC::GetLastSyncPts()
{
	return (last_syc_pts);
}
