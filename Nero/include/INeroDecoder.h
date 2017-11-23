/*
 * NeroIntelCE4x00Decoder.h
 *
 *  Created on: Aug 17, 2017
 *      Author: g360476
 */

#ifndef NERO_DECODER_H_
#define NERO_DECODER_H_
#include "NeroConstants.h"
#include "sys/types.h"
//#include "Observateur.h"
using namespace std;

class INeroDecoder /*: public Observateur*/{

public:

INeroDecoder(){};
~INeroDecoder(){}

virtual Nero_error_t   Init (size_t NeroAlgo)       {return (NERO_SUCCESS);};
virtual Nero_error_t   Reconfigure(size_t NeroAlgo) {return (NERO_SUCCESS);};
virtual Nero_error_t   UnInit() {return (NERO_SUCCESS);};
virtual Nero_error_t   Flush()  {return (NERO_SUCCESS);};
virtual Nero_error_t   Stop()   {return (NERO_SUCCESS);};
virtual Nero_error_t   Close()  {return (NERO_SUCCESS);};
virtual Nero_error_t   Pause()  {return (NERO_SUCCESS);};
virtual Nero_error_t   Play()   {return (NERO_SUCCESS);};
virtual Nero_error_t   Feed (uint8_t* payload, size_t payload_size, uint64_t nrd_pts, bool discontinuity){return (NERO_SUCCESS);};
virtual NeroDecoderState_t GetState(){return (NERO_DECODER_LAST);};
virtual uint64_t GetLastPts(){return (0);};
/* for testing to be used for PES injection */
virtual int            Getport(){return (0);};
virtual Nero_error_t NeroEventWait(NeroEvents_t *event){return (NERO_SUCCESS);};
};

#endif /* NERO_DECODER_H_ */
