/*
 * NeroIntelCE4x00AudioProcessor.h
 *
 *  Created on: Aug 18, 2017
 *      Author: g360476
 */
#ifndef NEROINTELCE4x00_AUDIO_PROCESSOR_H_
#define NEROINTELCE4x00_AUDIO_PROCESSOR_H_
#include "NeroConstants.h"
#include "NeroIntelCE4x00Constants.h"
extern "C"
{
#include "osal.h"
#include "ismd_core.h"
#include "ismd_global_defs.h"
#include "ismd_audio_defs.h"
#include "ismd_audio.h"
#include "psi_handler.h"
#include <sched.h>
}
using namespace std;
class NeroIntelCE4x00AudioProcessor
{

public:
    int GetHandle();
    static NeroIntelCE4x00AudioProcessor *instance()
    {
        if (!s_instance)
          s_instance = new NeroIntelCE4x00AudioProcessor;
        return s_instance;
    }
private:
NeroIntelCE4x00AudioProcessor();
~NeroIntelCE4x00AudioProcessor();
ismd_audio_processor_t audio_processor;
static NeroIntelCE4x00AudioProcessor *s_instance;
};

#endif /*NEROINTELCE4x00_AUDIO_PROCESSOR_H_*/
