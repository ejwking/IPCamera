
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


//####################################################################################################################################

class Frame::Impl
{
public:
	AVFrame* m_frame = nullptr;
};

Frame::Frame()
{
	m_Impl = std::make_unique<Impl>();
	m_Impl->m_frame = av_frame_alloc();
}

Frame::~Frame()
{
	if (m_Impl->m_frame)
		av_frame_free(&m_Impl->m_frame);
}

bool Frame::IsValid() const
{
	return m_Impl->m_frame && m_Impl->m_frame->data[0];
}

int Frame::Width() const
{
	return m_Impl->m_frame->width;
}

int Frame::Height() const
{
	return m_Impl->m_frame->height;
}

int Frame::Stride() const
{
	return m_Impl->m_frame->linesize[0];
}

const uint8_t* Frame::ScanLine(int y) const
{
	return m_Impl->m_frame->data[0] + y * m_Impl->m_frame->linesize[0];
}

const uint8_t* Frame::Data() const
{
	return m_Impl->m_frame->data[0];
}

int Frame::PixelFormat() const
{
	// Although it's FFmpeg's pixel format internally, returning it as an int keeps Frame.h free of FFmpeg types, and ImageConverter can cast it back to AVPixelFormat 
	// internally. It's a small compromise that keeps the public header clean while giving us exactly what we need for the next class.
	return m_Impl->m_frame->format;
}

const char* Frame::PixelFormatName() const
{
	return av_get_pix_fmt_name((AVPixelFormat)m_Impl->m_frame->format);
}

//####################################################################################################################################

class ImageConverter::Impl
{
public:
	SwsContext* m_sws = nullptr;
	int m_width = 0;
	int m_height = 0;
	AVPixelFormat m_sourceFormat = AV_PIX_FMT_NONE;
};

ImageConverter::ImageConverter()
{
	m_Impl = std::make_unique<Impl>();
}

ImageConverter::~ImageConverter()
{
	if (m_Impl->m_sws)
		sws_freeContext(m_Impl->m_sws);
}

bool ImageConverter::Convert(const Frame& source, Frame& destination, bool BGR/*=false*/)
{
	// Typical camera formats are, YUV420P, NV12, YUV422. Nobody wants to process those directly.
	// Instead we'll ask FFmpeg to convert the frame into RGB24. That's the purpose of libswscale.

	AVFrame* src = source.m_Impl->m_frame;
	AVFrame* dst = destination.m_Impl->m_frame;

	if (!src || !src->data[0])
		return false;

	//----------------------------------------------------------
	// Rebuild converter if source format changes
	//----------------------------------------------------------

	if (m_Impl->m_sws == nullptr ||
		m_Impl->m_width        != src->width ||
		m_Impl->m_height       != src->height ||
		m_Impl->m_sourceFormat != (AVPixelFormat)src->format)
	{
		if (m_Impl->m_sws)
			sws_freeContext(m_Impl->m_sws);

		// Create the RGB converter. This object performs the colour conversion.
		m_Impl->m_sws = sws_getContext(src->width, src->height, (AVPixelFormat)src->format,
									 src->width, src->height, 
									 BGR ? AV_PIX_FMT_BGR24 : AV_PIX_FMT_RGB24,
									 SWS_BILINEAR, nullptr, nullptr, nullptr);
		if (!m_Impl->m_sws)
			return false;

		m_Impl->m_width = src->width;
		m_Impl->m_height = src->height;
		m_Impl->m_sourceFormat = (AVPixelFormat)src->format;
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

	sws_scale(m_Impl->m_sws, src->data, src->linesize, 0, src->height, dst->data, dst->linesize);

	return true;
}

//####################################################################################################################################

class Camera::Impl
{
public:
	AVFormatContext* m_formatContext = nullptr;
	AVCodecContext* m_codecContext = nullptr;
	AVPacket* m_packet = nullptr;
	AVFrame* m_frame = nullptr;
	int m_videoStream = -1;
	Frame m_currentFrame;

	bool Open(const std::string & url);
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
	if (m_Impl->m_packet)
	{
		av_packet_free(&m_Impl->m_packet);
	}

	if (m_Impl->m_codecContext)
	{
		avcodec_free_context(&m_Impl->m_codecContext);
	}

	if (m_Impl->m_formatContext)
	{
		avformat_close_input(&m_Impl->m_formatContext);
	}

	m_Impl->m_videoStream = -1;
}

void Camera::GetVideoInfo()
{
	AVStream* stream = m_Impl->m_formatContext->streams[m_Impl->m_videoStream];
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

bool Camera::Impl::Open(const std::string& url)
{
	AVDictionary* options = nullptr;
	bool isRtsp = (url.rfind("rtsp://", 0) == 0);
	if (isRtsp)
	{
		// Use TCP instead of UDP. RTSP can transport video over UDP (lower latency) or TCP (more reliable). For cameras on a home network, I recommend TCP.
		av_dict_set(&options, "rtsp_transport", "tcp", 0);
		// If the camera is unplugged, I don't want Open() to hang for a long time. (5,000,000 microseconds = 5 seconds).
		// If the camera is on a local network, 5 seconds is plenty of time to wait for a response. If the camera is on the internet, you may want to increase this timeout.
		// Some newer FFmpeg builds use "timeout" instead of "stimeout", but if your build accepts stimeout, that's fine.
		av_dict_set(&options, "stimeout", "5000000", 0);
		// Increase the receive buffer, this is a 1 MB network buffer.
		av_dict_set(&options, "buffer_size", "1048576", 0);
	}

	int result = avformat_open_input(&m_formatContext, url.c_str(), nullptr, &options);

	// Free the dictionary.
	av_dict_free(&options);

	if (result < 0)
	{
		return false;
	}

	// This tells FFmpeg to inspect the file (or stream) and discover its contents.
	if (avformat_find_stream_info(m_formatContext, nullptr) < 0)
	{
		return false;
	}

	// Find the first video stream in the file (or stream).
	for (unsigned int i = 0; i < m_formatContext->nb_streams; i++)
	{
		AVStream* stream = m_formatContext->streams[i];
		if (stream->codecpar->codec_type == AVMEDIA_TYPE_VIDEO)
		{
			m_videoStream = i;
			break;
		}
	}
	if (m_videoStream == -1)
	{
		return false;
	}

	// Find the decoder for the video stream.
	// If the video is H.264, decoder will point to FFmpeg's H.264 decoder, etc etc.
	AVCodecParameters* codecPar = m_formatContext->streams[m_videoStream]->codecpar;
	const AVCodec* decoder = avcodec_find_decoder(codecPar->codec_id);
	if (!decoder)
	{
		return false;
	}

	// Create a decoder context.
	// Copy codec parameters from the stream to the codec context.
	// Think of the AVCodecContext as the decoder instance that maintains all the state needed to decode the video.
	m_codecContext = avcodec_alloc_context3(decoder);
	if (!m_codecContext)
	{
		return false;
	}

	// Copy the stream parameters.
	// The stream contains information like - Width, Height, Pixel format, Codec profile. Copy all of that into the decoder context.
	if (avcodec_parameters_to_context(m_codecContext, codecPar) < 0)
	{
		return false;
	}

	// Open the decoder.
	if (avcodec_open2(m_codecContext, decoder, nullptr) < 0)
	{
		return false;
	}

	// At this point the decoder is ready.
	// Allocate the packet and frame, these are reused for every decoded frame.
	// The packet is used to hold the compressed data read from the stream, and the frame is used to hold the decompressed data after decoding.
	m_packet = av_packet_alloc();
	m_frame  = av_frame_alloc();
	if (!m_packet || !m_frame)
	{
		return false;
	}
	// When Open() returns true, the camera (or file) is completely ready to decode.
	return true;
}

bool Camera::Open(const std::string& url)
{
	if(m_Impl->m_videoStream == -1)
	{
		if(!m_Impl->Open(url))
		{
			Close();
			return false;
		}
		GetVideoInfo();
	}
	// When Open() returns true, the camera (or file) is completely ready to decode.
	return true;
}

const Frame& Camera::CurrentFrame() const
{
	return m_Impl->m_currentFrame;
}

bool Camera::Grab()
{
	// For an MP4, reaching end-of-file means Grab() returns false.
	// For an RTSP stream, there is no end-of-file. Instead, av_read_frame() might fail temporarily because of a network hiccup.
	// To do:
	// make Grab() distinguish between:
	// End of file (for local files).
	// Temporary network errors (for RTSP), where it can retry instead of immediately giving up.
	// (I'll leave it until I actually see a stream interruption).

	while (av_read_frame(m_Impl->m_formatContext, m_Impl->m_packet) >= 0)
	{
		if (m_Impl->m_packet->stream_index != m_Impl->m_videoStream)
		{
			av_packet_unref(m_Impl->m_packet);
			continue;
		}

		if (avcodec_send_packet(m_Impl->m_codecContext, m_Impl->m_packet) < 0)
		{
			av_packet_unref(m_Impl->m_packet);
			continue;
		}

		av_packet_unref(m_Impl->m_packet);

		// so the frame is returned to an empty state before FFmpeg writes the next image into it..
		av_frame_unref(m_Impl->m_currentFrame.m_Impl->m_frame);

		int result = avcodec_receive_frame(m_Impl->m_codecContext, m_Impl->m_currentFrame.m_Impl->m_frame);

		if (result == 0)
		{
			return true;
		}
	}
	return false;
}



/*
// allocate storage for it.
int size = av_image_get_buffer_size(AV_PIX_FMT_RGB24, codecPar->width, codecPar->height, 1);
m_Impl->m_rgbBuffer = new uint8_t[size];

// and attach it to the RGB frame.
av_image_fill_arrays(m_Impl->m_rgbFrame->data, m_Impl->m_rgbFrame->linesize, m_Impl->m_rgbBuffer, AV_PIX_FMT_RGB24, codecPar->width, codecPar->height, 1);
*/
