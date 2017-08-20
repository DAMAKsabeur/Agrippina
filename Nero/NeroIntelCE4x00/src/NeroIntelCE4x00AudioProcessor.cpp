/*
 * NeroIntelCE4x00AudioProcessor.cpp
 *
 *  Created on: Aug 18, 2017
 *      Author: g360476
 */

#include "NeroIntelCE4x00AudioProcessor.h"
// Allocating and initializing NeroIntelCE4x00AudioProcessor's
// static data member.  The pointer is being
// allocated - not the object inself.
NeroIntelCE4x00AudioProcessor *NeroIntelCE4x00AudioProcessor::s_instance = NULL;


NeroIntelCE4x00AudioProcessor::NeroIntelCE4x00AudioProcessor()
{
    NERO_AUDIO_NOTICE ("NeroIntelCE4x00AudioProcessor creation ... \n");
    Nero_error_t     result = NERO_SUCCESS;
    ismd_result_t ismd_ret = ISMD_SUCCESS;
    ismd_ret= ismd_audio_open_processor(&audio_processor);
}

NeroIntelCE4x00AudioProcessor::~NeroIntelCE4x00AudioProcessor()
{
    NERO_AUDIO_NOTICE ("NeroIntelCE4x00AudioProcessor killer ... \n");
    Nero_error_t     result = NERO_SUCCESS;
    ismd_result_t ismd_ret = ISMD_SUCCESS;
    ismd_ret= ismd_audio_close_processor(audio_processor);
}

int NeroIntelCE4x00AudioProcessor::GetHandle(void)
{
	return ((int)audio_processor);
}
