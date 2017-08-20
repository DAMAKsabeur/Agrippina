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

NeroSTC::NeroSTC(uint8_t type)
{

	NERO_DEBUG("NeroSTC creator ... \n");
    m_NeroSTC = new NeroSTC_class(type);

}
NeroSTC::~NeroSTC()
{

	NERO_DEBUG("NeroSTC distuctor ... \n");
    delete m_NeroSTC;
    m_NeroSTC = NULL;

}
Nero_error_t NeroSTC::NeroSTCSetPlayBack()
{
	Nero_error_t result = NERO_SUCCESS;
	NERO_DEBUG("NeroSTCSetPlayBack ... \n");
	result = m_NeroSTC->NeroSTCSetPlayBack();
}

nero_clock_handle_t NeroSTC::NeroSTCGetClock()
{
	NERO_DEBUG("NeroSTCGetClock ... \n");
	return ((nero_clock_handle_t)m_NeroSTC->NeroSTCGetClock());
}
Nero_error_t NeroSTC::NeroSTCSetBaseTime(uint64_t time)
{
	NERO_DEBUG("NeroSTCSetBaseTime ... \n");
	return (m_NeroSTC->NeroSTCSetBaseTime(time));
}

uint64_t NeroSTC::NeroSTCGetBaseTime()
{
	NERO_DEBUG("NeroSTCGetBaseTime ... \n");
	return(m_NeroSTC->NeroSTCGetBaseTime());
}

Nero_stc_type_t NeroSTC::NeroSTCGetType()
{
	NERO_DEBUG("NeroSTCGetType ... \n");
	return(m_NeroSTC->NeroSTCGetType());
}
Nero_error_t NeroSTC::NeroSTCSetType(Nero_stc_type_t stc_type)
{
	NERO_DEBUG("NeroSTCSetType ... \n");
	return(m_NeroSTC->NeroSTCSetType(stc_type));
}
