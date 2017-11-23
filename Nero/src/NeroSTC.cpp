/*
 * NeroSTC.cpp
 *
 *  Created on: Aug 12, 2017
 *      Author: g360476
 */

#include "NeroSTC.h"

using namespace std;

/*************************************************************************
 * private:
**************************************************************************/



/*************************************************************************
 * public:
**************************************************************************/

NeroSTC::NeroSTC()
{
	NERO_DEBUG("NeroSTC creator ... \n");
    m_NeroSTC = new NeroSTC_class();
}

NeroSTC::NeroSTC(Nero_stc_type_t SyncMode)
{

	NERO_DEBUG("NeroSTC creator ... \n");
    m_NeroSTC = new NeroSTC_class(SyncMode);

}
NeroSTC::~NeroSTC()
{

	NERO_DEBUG("NeroSTC distuctor ... \n");
    delete m_NeroSTC;
    m_NeroSTC = NULL;

}
Nero_error_t NeroSTC::ConncectSources(NeroAudioDecoder* NeroAudioDecoder_ptr, NeroVideoDecoder* NeroVideoDecoder_ptr)
{

	NERO_DEBUG("NeroSTC ConncectSources ... \n");
	return(m_NeroSTC->ConncectSources(NeroAudioDecoder_ptr,NeroVideoDecoder_ptr));

}
Nero_error_t NeroSTC::DisconncectSources()
{
	NERO_DEBUG("NeroSTC ConncectSources ... \n");
	return(m_NeroSTC->DisconncectSources());
}
uint64_t NeroSTC::GetLastSyncPts()
{
	return(m_NeroSTC->GetLastSyncPts());
}

