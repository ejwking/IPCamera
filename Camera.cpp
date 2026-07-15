
#include "pch.h"
#include "Camera.h"


// https://github.com/FFmpeg/FFmpeg/blob/master/doc/examples/demux_decode.c

extern "C"
{
	#include <libavformat/avformat.h>
	#include <libavcodec/avcodec.h>
	#include <libavutil/avutil.h>
	#include <libavutil/imgutils.h>
	#include <libswscale/swscale.h>
}

class Frame::Impl
{
public:
	AVFrame* frame = nullptr;
};

class ImageConverter::Impl
{
public:
	SwsContext* sws = nullptr;
	int width = 0;
	int height = 0;
	AVPixelFormat sourceFormat = AV_PIX_FMT_NONE;
};


ImageConverter::ImageConverter()
{
	m_Impl = std::make_unique<Impl>();
}

ImageConverter::~ImageConverter()
{
	if (m_Impl->sws)
		sws_freeContext(m_Impl->sws);
}

bool ImageConverter::Convert(const Frame& source, Frame& destination)
{
	// Typical camera formats are, YUV420P, NV12, YUV422. Nobody wants to process those directly.
	// Instead we'll ask FFmpeg to convert the frame into RGB24. That's the purpose of libswscale.

	AVFrame* src = source.m_Impl->frame;
	AVFrame* dst = destination.m_Impl->frame;

	if (!src || !src->data[0])
		return false;

	//----------------------------------------------------------
	// Rebuild converter if source format changes
	//----------------------------------------------------------

	if (m_Impl->sws == nullptr ||
		m_Impl->width        != src->width ||
		m_Impl->height       != src->height ||
		m_Impl->sourceFormat != (AVPixelFormat)src->format)
	{
		if (m_Impl->sws)
			sws_freeContext(m_Impl->sws);

		m_Impl->sws = sws_getContext(src->width, src->height, (AVPixelFormat)src->format,
									 src->width, src->height, AV_PIX_FMT_RGB24, SWS_BILINEAR,
									 nullptr, nullptr, nullptr);

		if (!m_Impl->sws)
			return false;

		m_Impl->width = src->width;
		m_Impl->height = src->height;
		m_Impl->sourceFormat = (AVPixelFormat)src->format;
	}

	//----------------------------------------------------------
	// Allocate destination frame if necessary
	//----------------------------------------------------------

	if (dst->width  != src->width ||
		dst->height != src->height ||
		dst->format != AV_PIX_FMT_RGB24 ||
		dst->data[0] == nullptr)
	{
		av_frame_unref(dst);

		dst->format = AV_PIX_FMT_RGB24;
		dst->width  = src->width;
		dst->height = src->height;

		if (av_frame_get_buffer(dst, 32) < 0)
			return false;
	}

	//----------------------------------------------------------
	// Convert
	//----------------------------------------------------------

	sws_scale(m_Impl->sws, src->data, src->linesize, 0, src->height, dst->data, dst->linesize);

	return true;
}

Frame::Frame()
{
	m_Impl = std::make_unique<Impl>();
	m_Impl->frame = av_frame_alloc();
}

Frame::~Frame()
{
	if (m_Impl->frame)
		av_frame_free(&m_Impl->frame);
}

bool Frame::IsValid() const
{
	return m_Impl->frame && m_Impl->frame->data[0];
}

int Frame::Width() const
{
	return m_Impl->frame->width;
}

int Frame::Height() const
{
	return m_Impl->frame->height;
}

int Frame::Stride() const
{
	return m_Impl->frame->linesize[0];
}

const uint8_t* Frame::ScanLine(int y) const
{
	return m_Impl->frame->data[0] + y * m_Impl->frame->linesize[0];
}

int Frame::PixelFormat() const
{
	// Although it's FFmpeg's pixel format internally, returning it as an int keeps Frame.h free of FFmpeg types, and ImageConverter can cast it back to AVPixelFormat 
	// internally. It's a small compromise that keeps the public header clean while giving us exactly what we need for the next class.
	return m_Impl->frame->format;
}

const char* Frame::PixelFormatName() const
{
	return av_get_pix_fmt_name((AVPixelFormat)m_Impl->frame->format);
}


//##################################################################

class Camera::Impl
{
public:
	AVFormatContext* formatContext = nullptr;
	AVCodecContext* codecContext = nullptr;
	AVPacket* packet = nullptr;
	AVFrame* frame = nullptr;
	int videoStream = -1;
	Frame currentFrame;
};

Camera::Camera()
{
	m_Impl = new Impl;
}

Camera::~Camera()
{
	Close();
	delete m_Impl;
}

void Camera::Close()
{
	if (m_Impl->packet)
	{
		av_packet_free(&m_Impl->packet);
	}

	if (m_Impl->codecContext)
	{
		avcodec_free_context(&m_Impl->codecContext);
	}

	if (m_Impl->formatContext)
	{
		avformat_close_input(&m_Impl->formatContext);
	}

	m_Impl->videoStream = -1;
}

void Camera::GetVideoInfo()
{
	AVStream* stream = m_Impl->formatContext->streams[m_Impl->videoStream];
	AVCodecParameters* codec = stream->codecpar;
	const AVCodecDescriptor* desc = avcodec_descriptor_get(codec->codec_id);
	
	m_VideoInfo.width = codec->width;
	m_VideoInfo.height = codec->height;
	m_VideoInfo.fps = av_q2d(stream->avg_frame_rate);
	m_VideoInfo.codec_id = codec->codec_id;
	m_VideoInfo.codecName = desc ? desc->name : "Unknown";

	TRACE("\n\nWidth  : %d   ", m_VideoInfo.width);
	TRACE("\nHeight : %d   ", m_VideoInfo.height);
	TRACE("\nCodec ID : %d   ", m_VideoInfo.codec_id);
	TRACE("\nFPS : %f   ", m_VideoInfo.fps);
	TRACE("\nCodec : %s   ", m_VideoInfo.codecName.c_str());
}

bool Camera::Open(const std::string& url)
{
	if (avformat_open_input(&m_Impl->formatContext, url.c_str(), nullptr, nullptr) != 0)
	{
		return false;
	}

	// This tells FFmpeg to inspect the file (or stream) and discover its contents.
	if (avformat_find_stream_info(m_Impl->formatContext, nullptr) < 0)
	{
		return false;
	}

	// Find the first video stream in the file (or stream).
	for (unsigned int i = 0; i < m_Impl->formatContext->nb_streams; i++)
	{
		AVStream* stream = m_Impl->formatContext->streams[i];
		if (stream->codecpar->codec_type == AVMEDIA_TYPE_VIDEO)
		{
			m_Impl->videoStream = i;
			break;
		}
	}
	if (m_Impl->videoStream == -1)
	{
		return false;
	}

	GetVideoInfo();

	// Find the decoder for the video stream.
	// If the video is H.264, decoder will point to FFmpeg's H.264 decoder, etc etc.
	AVCodecParameters* codecPar = m_Impl->formatContext->streams[m_Impl->videoStream]->codecpar;
	const AVCodec* decoder = avcodec_find_decoder(codecPar->codec_id);
	if (!decoder)
	{
		return false;
	}

	// Create a decoder context.
	// Copy codec parameters from the stream to the codec context.
	// Think of the AVCodecContext as the decoder instance that maintains all the state needed to decode the video.
	m_Impl->codecContext = avcodec_alloc_context3(decoder);
	if (!m_Impl->codecContext)
	{
		return false;
	}

	// Copy the stream parameters.
	// The stream contains information like - Width, Height, Pixel format, Codec profile. Copy all of that into the decoder context.
	if (avcodec_parameters_to_context(m_Impl->codecContext, codecPar) < 0)
	{
		return false;
	}

	// Open the decoder.
	if (avcodec_open2(m_Impl->codecContext, decoder, nullptr) < 0)
	{
		return false;
	}

	// At this point the decoder is ready.
	// Allocate the packet and frame, these are reused for every decoded frame.
	// The packet is used to hold the compressed data read from the stream, and the frame is used to hold the decompressed data after decoding.
	m_Impl->packet = av_packet_alloc();
	m_Impl->frame  = av_frame_alloc();
	if (!m_Impl->packet || !m_Impl->frame)
	{
		return false;
	}

	// When Open() returns true, the camera (or file) is completely ready to decode.
	return true;
}

const Frame& Camera::CurrentFrame() const
{
	return m_Impl->currentFrame;
}

bool Camera::Grab()
{
	while (av_read_frame(m_Impl->formatContext, m_Impl->packet) >= 0)
	{
		if (m_Impl->packet->stream_index != m_Impl->videoStream)
		{
			av_packet_unref(m_Impl->packet);
			continue;
		}

		if (avcodec_send_packet(m_Impl->codecContext, m_Impl->packet) < 0)
		{
			av_packet_unref(m_Impl->packet);
			continue;
		}

		av_packet_unref(m_Impl->packet);

		// so the frame is returned to an empty state before FFmpeg writes the next image into it..
		av_frame_unref(m_Impl->currentFrame.m_Impl->frame);

		int result = avcodec_receive_frame(m_Impl->codecContext, m_Impl->currentFrame.m_Impl->frame);

		if (result == 0)
		{
			return true;
		}
	}

	return false;
}



/*

// Create the RGB converter.
// This object performs the colour conversion. It only has to be created once.
m_Impl->swsContext = sws_getContext(codecPar->width, codecPar->height, m_Impl->codecContext->pix_fmt,
codecPar->width, codecPar->height, AV_PIX_FMT_RGB24, SWS_BILINEAR,
nullptr, nullptr, nullptr);

// Allocate an RGB frame.
m_Impl->rgbFrame = av_frame_alloc();

// Now allocate storage for it.
int size = av_image_get_buffer_size(AV_PIX_FMT_RGB24, codecPar->width, codecPar->height, 1);
m_Impl->rgbBuffer = new uint8_t[size];

// and attach it to the RGB frame.
av_image_fill_arrays(m_Impl->rgbFrame->data, m_Impl->rgbFrame->linesize, m_Impl->rgbBuffer, AV_PIX_FMT_RGB24, codecPar->width, codecPar->height, 1);

*/
