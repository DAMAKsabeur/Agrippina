/*
 * NeroConstants.h
 *
 *  Created on: Jul 27, 2017
 *      Author: g360476
 */

#ifndef NEROCONSTANTS_H_
#define NEROCONSTANTS_H_
#include "stdint.h"
#include "sys/types.h"
#define INTELCE4x00ISMD_IMPL 1
#define NERO_DEBUG printf
#define NERO_ERROR printf
#if INTELCE4x00ISMD_IMPL
#define NeroAudioDecoder_class NeroIntelCE4x00AudioDecoder
#define NeroVideoDecoder_class NeroIntelCE4x00VideoDecoder
#define NeroSTC_class  NeroIntelCE4x00STC
//typedef NeroIntelCE4x00STC NeroSTC_class  
#endif
#define NERO_MAXEVENT_DATA 0x0A
typedef struct  ES_t
{
char * buffer ;
size_t size;
uint64_t pts;
bool discontinuity;
}ES_t;

typedef enum Nero_Event_Liste_t
{
    NERO_EVENT_FRAME_FLIPPED = 0x00   ,
    NERO_EVENT_UNDERFLOW              ,
    NERO_EVENT_UNDERFLOWRECOVRED      ,
    NERO_EVENT_PTS_VALUE_EARLY        ,
    NERO_EVENT_PTS_VALUE_LATE         ,
    NERO_EVENT_PTS_PTS_VALUE_RECOVERED,
    NERO_EVENT_LAST
}Nero_Event_Liste_t;

typedef struct NeroEvents_t
{
	Nero_Event_Liste_t header;
    uint64_t pts;
    char data[NERO_MAXEVENT_DATA];
}NeroEvents_t;

typedef enum {
	NERO_AUDIO_ES_PRIMERY = 0x00,  /** port ID for ES audio             */
    NERO_AUDIO_ES_SECONDARY,       /** port ID for ES audio description */
    NERO_AUDIO_UI_SOUND,           /** port ID for UI Sound             */
    NERO_AUDIO_TTS,                /** port ID for text to speatch      */
    NERO_AUDIO_INPUTS_LAST         /** last port ID                     */
} Nero_AudioID_t;

typedef enum {
	NERO_STC_FREERUN      = 0x00,
	NERO_STC_AUDIO_MASTER = 0x01,         /** port ID for ES audio             */
	NERO_STC_VIDEO_MASTER=  0x02,         /** port ID for ES audio description */
} Nero_stc_type_t;
typedef enum {
   NERO_CODEC_AUDIO_INVALID = 0x00, /**< - Invalid or Unknown algorithm*/
   NERO_CODEC_AUDIO_PCM           ,/**< - stream is raw linear PCM data*/
   NERO_CODEC_AUDIO_DVD_PCM       ,/**< - stream is linear PCM data coming from a DVD */
   NERO_CODEC_AUDIO_BLURAY_PCM    ,/**< - stream is linear PCM data coming from a BD (HDMV Audio) */
   NERO_CODEC_AUDIO_MPEG          ,/**< - stream uses MPEG-1 or MPEG-2 BC algorithm*/
   NERO_CODEC_AUDIO_AAC           ,/**< - stream uses MPEG-2 or MPEG-4 AAC algorithm*/
   NERO_CODEC_AUDIO_AAC_LOAS      ,/**< - stream uses MPEG-2 or MPEG-4 AAC algorithm with LOAS header format*/
   NERO_CODEC_AUDIO_DD            ,/**< - stream uses Dolby Digital (AC3) algorithm*/
   NERO_CODEC_AUDIO_DD_PLUS       ,/**< - stream uses Dolby Digital Plus (E-AC3) algorithm*/
   NERO_CODEC_AUDIO_TRUE_HD       ,/**< - stream uses Dolby TrueHD algorithm*/
   NERO_CODEC_AUDIO_DTS_HD        ,/**< - stream uses DTS High Definition audio algorithm */
   NERO_CODEC_AUDIO_DTS_HD_HRA    ,/**< - DTS-HD High Resolution Audio */
   NERO_CODEC_AUDIO_DTS_HD_MA     ,/**< - DTS-HD Master Audio */
   NERO_CODEC_AUDIO_DTS           ,/**< - stream uses DTS  algorithm*/
   NERO_CODEC_AUDIO_DTS_LBR       ,/**< - stream uses DTS Low BitRate decode algorithm*/
   NERO_CODEC_AUDIO_DTS_BC        ,/**< - stream uses DTS broadcast decode algorithm*/
   NERO_CODEC_AUDIO_WM9           ,/**< - stream uses Windows Media 9 algorithm*/
   NERO_CODEC_AUDIO_DDC           ,/**< - stream uses DD/DDP algorithm and decoded by DDC decoder*/
   NERO_CODEC_AUDIO_DDT           ,/**< - stream uses AAC(up to AAC-HE V2)algorithm and decoded by DDT decoder*/
   NERO_CODEC_AUDIO_DRA           ,/**< - stream uses Digital Rise Audio algorithm*/
   NERO_CODEC_AUDIO_COUNT          /**< - Count of enum members.*/
} Nero_audio_codec_t;

/** Codecs supported by streaming media drivers */
typedef enum {
	NERO_CODEC_VIDEO_INVALID = 0, /**< A known invalid codec type. */
	NERO_CODEC_VIDEO_MPEG2   , /**< MPEG2 video codec type. */
	NERO_CODEC_VIDEO_H264    , /**< H264 video codec type. */
	NERO_CODEC_VIDEO_VC1     , /**< VC1 video codec type. */
	NERO_CODEC_VIDEO_MPEG4   , /**< MPEG4 video codec type. */
	NERO_CODEC_VIDEO_JPEG      /**< JPEG video codec type. */
} Nero_video_codec_t;

typedef enum {
	NERO_DECODER_STOP = 0x00,
	NERO_DECODER_PAUSE      ,
    NERO_DECODER_PLAY       ,
    NERO_DECODER_LAST
} NeroDecoderState_t;

typedef enum {
	NERO_AUDIO_HDMI_OUTPUT = 0x00,  /** HDMI Audio Output            */
    NERO_AUDIO_SPDIF_OUTPUT,       /** SPDIF Audio Output            */
    NERO_AUDIO_ANALOG_OUTPUT,      /** Analog Audio Output           */
    NERO_AUDIO_OUTPUT_LAST        /** last Audio Output              */
} Nero_AudioOutput_t;

typedef enum {
	NERO_SUCCESS = 0,          /**< No error occurred. The function executed successfully */
    /* handle errors */
    NERO_ERROR_INTERNAL,
    NERO_ERROR_BAD_HANDLE,      /**< Invalid handle. */
    NERO_ERROR_BAD_PARAMETER,
    /* limitations */
    NERO_ERROR_NOT_IMPLEMENTED,
    NERO_ERROR_NOT_CAPABLE,
    NERO_ERROR_LAST             /**< Maximum enum value for generic error codes. (This error will never occur) */
} Nero_error_t;


#endif /* NEROCONSTANTS_H_ */
