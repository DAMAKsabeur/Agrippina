/*
 * NeroVideoDecoder.cpp
 *
 *  Created on: Aug 12, 2017
 *      Author: g360476
 */
#include "NeroVideoDecoder.h"
using namespace std;
/* private functions */

/* public functions */
 /**1st constructor without stc: this shall create an internal STC to play video in free run mode
  * Free run driven clock mode */

NeroVideoDecoder::NeroVideoDecoder()
{
    internal_clock = true;
    stc =  new NeroSTC();
    m_NeroVideoDecoder = new NeroVideoDecoder_class(stc);

}

/**2end constructor with stc: this shall use passed STC to play video in stc master mode
 * stc master */

NeroVideoDecoder::NeroVideoDecoder(NeroSTC* m_NeroSTC)
{
    internal_clock = false;
    stc = m_NeroSTC;
    m_NeroVideoDecoder = new NeroVideoDecoder_class(stc);
}

/** video decoder distructor */

NeroVideoDecoder::~NeroVideoDecoder()
{
	delete m_NeroVideoDecoder;
	m_NeroVideoDecoder = NULL;
	if (internal_clock == true)
	{
		delete stc;
	}

}

/** video decoder: setup the video plane */

Nero_error_t NeroVideoDecoder::SetupPlane()
{

	Nero_error_t result = NERO_SUCCESS;
	result = m_NeroVideoDecoder->NeroVideoDecoderSetupPlane();
    return (result);
}

/** video decoder: resize the  video Layer */
Nero_error_t NeroVideoDecoder::ResizeLayer(size_t w,size_t h,size_t x,size_t y)
{

	Nero_error_t result = NERO_SUCCESS;
	result = m_NeroVideoDecoder->NeroVideoDecoderResizeLayer(w,h,x,y);
    return (result);
}

/** video decoder: init the decoder to play NerovideoAlgo codec stream */

Nero_error_t NeroVideoDecoder::Init(size_t NeroAlgo)
{
	Nero_error_t result = NERO_SUCCESS;
	Nero_video_codec_t NerovideoAlgo = (Nero_video_codec_t) NeroAlgo;
	result = m_NeroVideoDecoder->NeroVideoDecoderInit(NerovideoAlgo);
    return (result);
}

/** video decoder: init the decoder to reconfigure NerovideoAlgo codec stream */

Nero_error_t NeroVideoDecoder::Reconfigure(size_t NeroAlgo)
{

	Nero_video_codec_t NerovideoAlgo = (Nero_video_codec_t) NeroAlgo;
	Nero_error_t result = NERO_SUCCESS;
	result = m_NeroVideoDecoder->NeroVideoDecoderReconfigure(NerovideoAlgo);
	return (result);
}

/** video decoder: uninit the decoder from playing NerovideoAlgo codec stream */

Nero_error_t NeroVideoDecoder::UnInit()
{
	Nero_error_t result = NERO_SUCCESS;
	result = m_NeroVideoDecoder->NeroVideoDecoderUnInit();
	return (result);
}
/** video decoder: Fluch the video decoder pipeline */

Nero_error_t NeroVideoDecoder::Flush()
{
	Nero_error_t result = NERO_SUCCESS;
	result = m_NeroVideoDecoder->NeroVideoDecoderFlush();
	return (result);
}

/** video decoder: stop the video decoder pipeline */

Nero_error_t NeroVideoDecoder::Stop()
{
	Nero_error_t result = NERO_SUCCESS;
	result = m_NeroVideoDecoder->NeroVideoDecoderStop();
	return (result);
}

/** video decoder: Close the video decoder pipeline */

Nero_error_t NeroVideoDecoder::Close()
{
	Nero_error_t result = NERO_SUCCESS;
	result = m_NeroVideoDecoder->NeroVideoDecoderClose();
	return (result);
}

/** video decoder: pause the video decoder pipeline */

Nero_error_t NeroVideoDecoder::Pause()
{
	Nero_error_t result = NERO_SUCCESS;
	result = m_NeroVideoDecoder->NeroVideoDecoderPause();
	return (result);
}
/** video decoder: Play the video decoder pipeline */

Nero_error_t NeroVideoDecoder::Play()
{
	Nero_error_t result = NERO_SUCCESS;
	result = m_NeroVideoDecoder->NeroVideoDecoderPlay();
	return (result);
}

Nero_error_t NeroVideoDecoder::Feed(uint8_t* payload, size_t payload_size, uint64_t pts, bool discontinuity)
{
	Nero_error_t result = NERO_SUCCESS;
	result = m_NeroVideoDecoder->NeroVideoDecoderFeed(payload, payload_size, pts, discontinuity);
	return (result);
}

/** video decoder: Play the video decoder pipeline */

uint64_t NeroVideoDecoder::GetLastPts()
{
    return((uint64_t)m_NeroVideoDecoder->NeroVideoDecoderGetLastPts());
}

NeroDecoderState_t NeroVideoDecoder::GetState()
{
	return(m_NeroVideoDecoder->NeroVideoDecoderGetState());
}

/* for testing mode */
/* demux injection mode PES injection */
int NeroVideoDecoder::Getport()
{
	return((int)m_NeroVideoDecoder->NeroVideoDecoderGetport());
}

