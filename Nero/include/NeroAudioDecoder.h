#ifndef NERO_AUDIO_DECODER_H_
#define NERO_AUDIO_DECODER_H_
#include "NeroConstants.h"
#include "NeroSTC.h"
#include "INeroDecoder.h"
#include "NeroIntelCE4x00AudioDecoder.h"
#include "Observable.h"
using namespace std;

class NeroAudioDecoder : INeroDecoder , public Observable  {
public:

NeroAudioDecoder();
NeroAudioDecoder(NeroSTC* NeroSTC_ptr);
~NeroAudioDecoder();

Nero_error_t Init (size_t NeroAudioAlgo);
Nero_error_t Reconfigure(size_t NeroAudioAlgo);
Nero_error_t UnInit();

Nero_error_t Flush();
Nero_error_t Stop();
Nero_error_t Close();
Nero_error_t Pause();
Nero_error_t Play();
Nero_error_t Feed(uint8_t* payload, size_t payload_size, uint64_t pts, bool discontinuity);

NeroDecoderState_t GetState();

/* for testing */
int Getport();
virtual void Update(const Observable* observable);
Info Statut(void) const;
private:

/* private functions */
void UpdateStatus();
/* private variables */
NeroAudioDecoder_class* m_NeroAudioDecoder;
bool internal_clock;
NeroSTC* stc;
Info evt;
};
#endif /*NERO_AUDIO_DECODER_H_*/
