/*
 * NeroVideoDecoder.h
 *
 *  Created on: Aug 12, 2017
 *      Author: g360476
 */

#ifndef NERO_VIDEO_DECODER_H_
#define NERO_VIDEO_DECODER_H_
#include "NeroConstants.h"
#include "NeroSTC.h"
#include "INeroDecoder.h"
#include "NeroIntelCE4x00VideoDecoder.h"

using namespace std;

#define PTS_START_VALUE 0
#define INPUT_BUF_SIZE (32*1024) /* inject up to 32K of audio data per buffer*/

using namespace std;

class NeroVideoDecoder : INeroDecoder{
public:
NeroVideoDecoder();
NeroVideoDecoder(NeroSTC* NeroSTC_ptr);
~NeroVideoDecoder();

Nero_error_t SetupPlane();
Nero_error_t ResizeLayer(size_t w,size_t h,size_t x,size_t y);

virtual Nero_error_t Init (size_t NeroAlgo);
virtual Nero_error_t Reconfigure(size_t NeroAlgo);
virtual Nero_error_t UnInit();

virtual Nero_error_t Flush();
virtual Nero_error_t Stop();
virtual Nero_error_t Close();
virtual Nero_error_t Pause();
virtual Nero_error_t Play();
virtual Nero_error_t Feed(uint8_t* payload, size_t payload_size, uint64_t pts, bool discontinuity);
virtual uint64_t GetLastPts();
virtual NeroDecoderState_t GetState();
/* for testing */
virtual int Getport();
virtual void Update(const Observable* observable);
private:
/* private functions */

/* private variables */

NeroVideoDecoder_class* m_NeroVideoDecoder;
bool internal_clock;
NeroSTC* stc;
};
#endif /* NERO_VIDEO_DECODER_H_ */
