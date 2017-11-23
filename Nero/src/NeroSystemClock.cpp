/*
 * (c) 1997-2016 Netflix, Inc.  All content herein is protected by
 * U.S. copyright and other applicable intellectual property laws and
 * may not be copied without the express permission of Netflix, Inc.,
 * which reserves all rights.  Reuse of any of this content for any
 * purpose without the permission of Netflix, Inc. is strictly
 * prohibited.
 */
#include "NeroSystemClock.h"


using namespace std;

/*static snd_mixer_t *handle = NULL;
static snd_mixer_elem_t* elem = NULL;
static snd_mixer_selem_id_t *sid = NULL;
static bool isInit = false;*/

Nero_error_t NeroSystemClock::init()
{
	NeroIntelCE4x00SystemClock::init();
}

Nero_error_t NeroSystemClock::fini()
{
	NeroIntelCE4x00SystemClock::fini();
}
