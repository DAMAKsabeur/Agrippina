/*
 * Nero_main.cpp
 *
 *  Created on: Aug 10, 2017
 *      Author: g360476
 */
#include "../__NeroVideoDecoder.h"
#include "NeroAudioDecoder.h"
#include "NeroDemux.h"
#include "NeroClock.h"
#include "NeroConstants.h"
#include "NeroIntelCE4x00Constants.h"

#define PID_INVALID 0xffff

#define AUDIO_PID0 4352
#define AUDIO_ALGO NERO_CODEC_AUDIO_AAC
#define VIDEO_ALGO NERO_CODEC_VIDEO_H264
#define VIDEO_PID 4113
#define PCR_PID 4113
#define FILENAME "/Scrat_Gone_Nutty_1080i.ts"
#define READ_SIZE 8*1024
//void *kb_handler(void *);
void *feeder_thread(void *args);

//static int keyboard_thread_exit = 0;
static int main_thread_exit = 0;
//static os_thread_t keyboard_thread;
static os_thread_t input_thread;
static int is_pause;
   
/*void *kb_handler(void *unused)
{
   int key;
   unused = NULL;//compiler requires this
   int resized = 0;
   ismd_result_t ismd_ret = ISMD_SUCCESS;
   ismd_time_t original_base_time,orginal_clock_time,curr_clock;

   while(! keyboard_thread_exit) {
      key = getchar();
      switch (key) {
         case 'x': //x
         case 'q': //x
            keyboard_thread_exit = 1;
            printf("exit requested \n");
            break;
         case ' ': //space

             if (0 == resized) {
                 g_x = 1237;
                 g_y = 51;
                 g_width = 576;
                 g_height = 324;
                 resized = 1;
             } else {
                 g_x = 0;
                 g_y = 0;
                 g_width = 1920;
                 g_height = 1080;
                 resized = 0;
             }
             _TRY(ismd_ret,resize_layer,(layer, g_x, g_y, g_width, g_height));
             break;
         case 'p':
             if(!is_pause)
             {
                is_pause=1;
                pause_audio_pipeline();
                pause_video_pipeline();
                pause_demux_pipeline();
                _TRY(ismd_ret,ismd_clock_get_time,(clock_handle, &orginal_clock_time));
                _TRY(ismd_ret,ismd_dev_get_stream_base_time,(vidrend_handle, &original_base_time));
             }
             break;
         case 'r':
             if(is_pause)
             {
                 ismd_dev_state_t state;
                 is_pause=0;

                 _TRY(ismd_ret,ismd_dev_get_state,(vidrend_handle,&state));
                _TRY(ismd_ret,ismd_clock_get_time,(clock_handle, &curr_clock));
                original_base_time += curr_clock-orginal_clock_time+3000;
                 if(state != ISMD_DEV_STATE_PAUSE)
                 {
                    _TRY(ismd_ret,ismd_vidsink_set_state,(vidsink_handle,ISMD_DEV_STATE_PAUSE));
                 }
                _TRY(ismd_ret,ismd_vidsink_set_base_time,(vidsink_handle,original_base_time));
                _TRY(ismd_ret,ismd_dev_set_stream_base_time,(demux_stream_handle, original_base_time));
                _TRY(ismd_ret,ismd_dev_set_stream_base_time,(audio_handle, original_base_time));
                 start_audio_pipeline();
                 start_video_pipeline();
                 start_demux_pipeline();
                 if(state != ISMD_DEV_STATE_PAUSE)
                    _TRY(ismd_ret,ismd_vidsink_flush,(vidsink_handle));
             }
             break;

         case '\n':
         case '\r':
             break;
         default:
            printf("unrecognized key: %d\n", key);
      }
   }

error:
    main_thread_exit = 1;

   return NULL;
}
*/

void *feeder_thread(void *args)
{
   FILE *fp;
   char buf[READ_SIZE];
   ismd_buffer_handle_t ismd_buffer_handle;
   ismd_result_t smd_ret;
   ismd_buffer_descriptor_t ismd_buf_desc;
   uint8_t *buf_ptr;
   NeroDemux* Demux = (NeroDemux *)args;


   fp = fopen(FILENAME, "rb");
   if (fp == 0) {
      OS_INFO("could not open input");
      OS_ASSERT(0);
   }

   while (fread(buf, 1, READ_SIZE, fp) > 0) {
	   Demux->NeroDemuxFeed(buf,READ_SIZE);
	//printf("feeder feeder \n");
   }
   

//error:
    fclose(fp);
   return NULL;
}

int main (void) {

   ismd_result_t ismd_ret;
   Nero_error_t result = NERO_SUCCESS;
   NeroClock*               Clock = new NeroClock();
   NeroVideoDecoder* VideoDecoder = new NeroVideoDecoder(Clock->NeroClockGetSTCHandle());
   NeroAudioDecoder* AudioDecoer  = new NeroAudioDecoder(Clock->NeroClockGetSTCHandle());
   NeroDemux*              Demux  = new NeroDemux(Clock->NeroClockGetSTCHandle());


  // os_thread_create(&keyboard_thread, kb_handler, NULL, 0, 0, "keyboard thread");

   //_TRY(ismd_ret, VideoDecoder->setup_plane,());
   result = VideoDecoder->NeroVideoDecoderSetupPlane();
   if(result != NERO_SUCCESS)
   {
		printf("ERROR: NeroVideoDecoderSetupPlane fail (%d)",result);
       goto error;
   }

    
  /* result = NeroClockPtr.NeroClockInit();
    if(result != NERO_SUCCESS)
    {
		printf("ERROR: NeroClockInit fail (%d)",result);
        goto error;
    }*/
    result = VideoDecoder->NeroVideoDecoderInit(VIDEO_ALGO);
    if(result != NERO_SUCCESS)
    {
		printf("ERROR: NeroVideoDecoderInit fail (%d)",result);
        goto error;
    }
    /* init audio */
    result = AudioDecoer->NeroAudioDecoderInit(AUDIO_ALGO);
    if(result != NERO_SUCCESS)
    {
		printf("ERROR: NeroAudioDecoderInit fail (%d)",result);
        goto error;
    }


   // _TRY(ismd_ret,NeroInitVideo , (VIDEO_ALGO));
    /* init demux */
    result = Demux->NeroDemuxInit(AUDIO_PID0, VIDEO_PID ,PCR_PID, AudioDecoer->NeroIntelCE4x00AudioDecoderGetport() ,VideoDecoder->NeroIntelCE4x00VideoDecoderGetport());
    if(result != NERO_SUCCESS)
    {
		printf("ERROR: NeroDemuxInit fail (%d)",result);
        goto error;
    }
   // _TRY(ismd_ret,NeroInitDemux , (AUDIO_PID0,VIDEO_PID,PCR_PID));
   //_TRY(ismd_ret, build_file_pipeline, (AUDIO_PID0, VIDEO_PID, , AUDIO_ALGO, VIDEO_ALGO));
   //_TRY(ismd_ret, send_new_segment,());
   os_thread_create(&input_thread, feeder_thread,(void*)Demux, 0, 0, "file_in thread");

  while (! main_thread_exit) {
      os_sleep(20);
   }

error:
   //_TRY(ismd_ret, NeroUnInitdemux,());
   //_TRY(ismd_ret, NeroUnInitVideo,());

   result = AudioDecoer->NeroAudioDecoderUnInit();
   if (result != NERO_SUCCESS)
   {
	   printf("ERROR: NeroAudioDecoderUnInit fail (%d)",result);
   }

   result = VideoDecoder->NeroVideoDecoderUnInit();
      if (result != NERO_SUCCESS)
   {
	   printf("ERROR: NeroVideoDecoderUnInit fail (%d)",result);
   }
   //result = VideoDecoder->NeroVideo
   result = Demux->NeroDemuxUnInit();
      if (result != NERO_SUCCESS)
   {
	   printf("ERROR: NeroDemuxUnInit fail (%d)",result);
   }
   //delete &NeroClockPtr;
   delete Clock;
   delete AudioDecoer;
   delete VideoDecoder;
   delete Demux;

   //_TRY(ismd_ret, cleanup_resources,());
   return 0;
}

