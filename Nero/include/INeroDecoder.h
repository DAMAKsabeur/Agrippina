/*
 * NeroIntelCE4x00Decoder.h
 *
 *  Created on: Aug 17, 2017
 *      Author: g360476
 */

#ifndef NERO_DECODER_H_
#define NERO_DECODER_H_

#include "NeroSTC.h"
#include "NeroConstants.h"
#include "Observateur.h"
using namespace std;

class INeroDecoder : public Observateur{

public:

/*NeroDecoder(){};
NeroDecoder(NeroSTC* NeroSTC_ptr){};
~NeroDecoder(){};*/
INeroDecoder(){};
INeroDecoder(NeroSTC* NeroSTC_ptr){};
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
virtual NeroDecoderState_t GetState(){return (NERO_AUDIO_LAST);};
/* for testing to be used for PES injection */
virtual int            Getport(){return (0);};

};

#endif /* NERO_DECODER_H_ */
