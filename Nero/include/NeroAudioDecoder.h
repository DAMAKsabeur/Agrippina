#ifndef NERO_AUDIO_DECODER_H_
#define NERO_AUDIO_DECODER_H_
#include "NeroConstants.h"
#include "INeroDecoder.h"
#include "NeroIntelCE4x00AudioDecoder.h"

using namespace std;

class NeroAudioDecoder : INeroDecoder {
public:

NeroAudioDecoder();
~NeroAudioDecoder();

virtual Nero_error_t Init (size_t NeroAudioAlgo);
virtual Nero_error_t Reconfigure(size_t NeroAudioAlgo);
virtual Nero_error_t UnInit();

virtual Nero_error_t Flush();
virtual Nero_error_t Stop();
virtual Nero_error_t Close();
virtual Nero_error_t Pause();
virtual Nero_error_t Play();
virtual uint64_t GetLastPts();
virtual Nero_error_t Feed(uint8_t* payload, size_t payload_size, uint64_t pts, bool discontinuity);
virtual NeroDecoderState_t GetState();
virtual Nero_error_t NeroEventWait(NeroEvents_t *event);
/* for testing */
int Getport();
NeroBlockingQueue AudioESQueue;
private:
/* private functions */

/* private variables */
NeroAudioDecoder_class* m_NeroAudioDecoder;
bool internal_clock;
};
#endif /*NERO_AUDIO_DECODER_H_*/
