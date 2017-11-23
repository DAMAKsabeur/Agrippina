#include "NeroAudioDecoder.h"


using namespace std;


/* private functions */


/* public functions */
 
NeroAudioDecoder:: NeroAudioDecoder()
{

    internal_clock = true;
    m_NeroAudioDecoder = new NeroAudioDecoder_class();

}


/** audio decoder distructor */

NeroAudioDecoder::~NeroAudioDecoder()
{
	delete m_NeroAudioDecoder;
	m_NeroAudioDecoder = NULL;
}

/** audio decoder: init the decoder to play NeroAudioAlgo codec stream */

Nero_error_t NeroAudioDecoder::Init(size_t NeroAlgo)
{
	Nero_audio_codec_t NeroAudioAlgo = (Nero_audio_codec_t) NeroAlgo;
	Nero_error_t result = NERO_SUCCESS;
	result = m_NeroAudioDecoder->NeroAudioDecoderInit(NeroAudioAlgo);
    return (result);
}

/** audio decoder: init the decoder to reconfigure NeroAudioAlgo codec stream */

Nero_error_t NeroAudioDecoder::Reconfigure(size_t NeroAlgo)
{
	Nero_error_t result = NERO_SUCCESS;
	Nero_audio_codec_t NeroAudioAlgo = (Nero_audio_codec_t) NeroAlgo;
	result = m_NeroAudioDecoder->NeroAudioDecoderReconfigure(NeroAudioAlgo);
	return (result);
}

/** audio decoder: uninit the decoder from playing NeroAudioAlgo codec stream */

Nero_error_t NeroAudioDecoder::UnInit()
{
	Nero_error_t result = NERO_SUCCESS;
	result = m_NeroAudioDecoder->NeroAudioDecoderUnInit();
	return (result);
}
/** audio decoder: Fluch the audio decoder pipeline */

Nero_error_t NeroAudioDecoder::Flush()
{
	Nero_error_t result = NERO_SUCCESS;
	result = m_NeroAudioDecoder->NeroAudioDecoderFlush();
	return (result);
}

/** audio decoder: stop the audio decoder pipeline */

Nero_error_t NeroAudioDecoder::Stop()
{
	Nero_error_t result = NERO_SUCCESS;
	result = m_NeroAudioDecoder->NeroAudioDecoderStop();
	return (result);
}

/** audio decoder: pause the audio decoder pipeline */

Nero_error_t NeroAudioDecoder::Pause()
{
	Nero_error_t result = NERO_SUCCESS;
	result = m_NeroAudioDecoder->NeroAudioDecoderPause();
	return (result);
}

/** audio decoder: close the audio decoder pipeline */

Nero_error_t NeroAudioDecoder::Close()
{
	Nero_error_t result = NERO_SUCCESS;
	result = m_NeroAudioDecoder->NeroAudioDecoderClose();
	return (result);
}

/** audio decoder: Play the audio decoder pipeline */

Nero_error_t NeroAudioDecoder::Play()
{
	Nero_error_t result = NERO_SUCCESS;
	result = m_NeroAudioDecoder->NeroAudioDecoderPlay();
	return (result);
}

Nero_error_t NeroAudioDecoder::Feed(uint8_t* payload, size_t payload_size, uint64_t pts, bool discontinuity)
{
	Nero_error_t result = NERO_SUCCESS;
	result = m_NeroAudioDecoder->NeroAudioDecoderFeed(payload, payload_size, pts,discontinuity);
	return (result);
}

/** audio decoder: Play the audio decoder pipeline */

NeroDecoderState_t NeroAudioDecoder::GetState()
{
	return(m_NeroAudioDecoder->NeroAudioDecoderGetState());
}

uint64_t NeroAudioDecoder::GetLastPts()
{
    return((uint64_t)m_NeroAudioDecoder->NeroAudioDecoderGetLastPts());
}
/* for testing mode */
/* demux injection mode PES injection */
int NeroAudioDecoder::Getport()
{
	return((int)m_NeroAudioDecoder->NeroAudioDecoderGetport());
}
Nero_error_t NeroAudioDecoder::NeroEventWait(NeroEvents_t *event)
{
	return((Nero_error_t)m_NeroAudioDecoder->NeroAudioDecoderEventWait(event));
}
