
#include "pch.h"
#include "Camera.h"
#include <thread>

// FFmpeg examples https://github.com/FFmpeg/FFmpeg/blob/master/doc/examples/demux_decode.c
extern "C"
{
	// FFmpeg headers:	..written in C, so tell the C++ compiler to treat the headers as C code using the extern "C" linkage 
	// specification. It prevents name mangling and allows the C++ code to link correctly with the C functions.
	#include <libavformat/avformat.h>
	#include <libavcodec/avcodec.h>
	#include <libavutil/avutil.h>
	#include <libavutil/imgutils.h>
	#include <libswscale/swscale.h>
}

//####################################################################################################################################

class CFrame::Impl
{
public:
	AVFrame* m_frame = nullptr;
};

CFrame::CFrame()
{
	m_Impl = std::make_unique<Impl>();
	m_Impl->m_frame = av_frame_alloc();
}

CFrame::~CFrame()
{
	if (m_Impl->m_frame)
		av_frame_free(&m_Impl->m_frame);
}

bool CFrame::IsValid() const
{
	return m_Impl->m_frame && m_Impl->m_frame->data[0];
}

int CFrame::Width() const
{
	return m_Impl->m_frame->width;
}

int CFrame::Height() const
{
	return m_Impl->m_frame->height;
}

int CFrame::LineSize() const
{
	return m_Impl->m_frame->linesize[0];
}

const uint8_t* CFrame::ScanLine(int y) const
{
	return m_Impl->m_frame->data[0] + (y * m_Impl->m_frame->linesize[0]);
}

const uint8_t* CFrame::Data() const
{
	return m_Impl->m_frame->data[0];
}

int CFrame::PixelFormat() const
{
	// Although it's FFmpeg's pixel format internally, returning it as an int keeps Frame.h free of FFmpeg types, and CImageConverter can cast it back to AVPixelFormat 
	// internally. It's a small compromise that keeps the public header clean while giving us exactly what we need for the next class.
	return m_Impl->m_frame->format;
}

const char* CFrame::PixelFormatName() const
{
	return av_get_pix_fmt_name((AVPixelFormat)m_Impl->m_frame->format);
}

//####################################################################################################################################

class CImageConverter::Impl
{
public:
	SwsContext* m_sws = nullptr;
	int m_width = 0;
	int m_height = 0;
	AVPixelFormat m_sourceFormat = AV_PIX_FMT_NONE;
};

CImageConverter::CImageConverter()
{
	m_Impl = std::make_unique<Impl>();
}

CImageConverter::~CImageConverter()
{
	if (m_Impl->m_sws)
		sws_freeContext(m_Impl->m_sws);
}

bool CImageConverter::Convert(const CFrame& source, CFrame& destination, bool BGR/*=false*/)
{
	// Typical camera formats are, YUV420P, NV12, YUV422. Nobody wants to process those directly.
	// Instead we'll ask FFmpeg to convert the frame into RGB24. That's the purpose of libswscale.
	AVFrame* src = source.m_Impl->m_frame;
	AVFrame* dst = destination.m_Impl->m_frame;

	if (!src || !src->data[0])
		return false;

	// Rebuild converter if source format changes
	if (m_Impl->m_sws    == nullptr ||
		m_Impl->m_width  != src->width ||
		m_Impl->m_height != src->height ||
		m_Impl->m_sourceFormat != (AVPixelFormat)src->format)
	{
		if (m_Impl->m_sws)
			sws_freeContext(m_Impl->m_sws);
		// Create the RGB converter. This object performs the colour conversion.
		m_Impl->m_sws = sws_getContext(src->width, src->height, (AVPixelFormat)src->format, src->width, src->height, 
										BGR ? AV_PIX_FMT_BGR24 : AV_PIX_FMT_RGB24,
										SWS_BILINEAR, nullptr, nullptr, nullptr);
		if (!m_Impl->m_sws)
			return false;
		m_Impl->m_width = src->width;
		m_Impl->m_height = src->height;
		m_Impl->m_sourceFormat = (AVPixelFormat)src->format;
	}

	// Allocate destination frame if necessary
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

	// Convert
	sws_scale(m_Impl->m_sws, src->data, src->linesize, 0, src->height, dst->data, dst->linesize);
	return true;
}

//####################################################################################################################################

class CCamera::Impl
{
public:
	AVFormatContext* m_formatContext = nullptr;
	AVCodecContext* m_codecContext = nullptr;
	AVPacket* m_packet = nullptr;
	AVFrame* m_frame = nullptr;
	int m_videoStream = -1;
	CFrame m_currentFrame;

	// hide the implementation methods, not just data. Otherwise even though it could be private in CCamera, this helper function would be in the public interface of CCamera.
	// CCamera class should remain clean and focused on its public interface.
	bool Open(const std::string & url);
};

CCamera::CCamera()
{
	m_Impl = std::make_unique<Impl>();
}

CCamera::~CCamera()
{
	Close();
}

void CCamera::Close()
{
	// Every xxx_alloc() or xxx_open() in Open() should have one matching xxx_free() or xxx_close() in Close().
	if (m_Impl->m_packet)
		av_packet_free(&m_Impl->m_packet);
	if (m_Impl->m_frame)
		av_frame_free(&m_Impl->m_frame);
	if (m_Impl->m_codecContext)
		avcodec_free_context(&m_Impl->m_codecContext);
	if (m_Impl->m_formatContext)
		avformat_close_input(&m_Impl->m_formatContext);

	m_Impl->m_videoStream = -1;
}

void CCamera::GetVideoInfo()
{
	AVStream* stream = m_Impl->m_formatContext->streams[m_Impl->m_videoStream];
	AVCodecParameters* codec = stream->codecpar;
	const AVCodecDescriptor* desc = avcodec_descriptor_get(codec->codec_id);
	
	m_VideoInfo.width = codec->width;
	m_VideoInfo.height = codec->height;
	m_VideoInfo.fps = av_q2d(stream->avg_frame_rate);
	m_VideoInfo.codec_id = codec->codec_id;
	m_VideoInfo.codecName = desc ? desc->name : "Unknown";

	// temp, remove this, TRACE shouldnt be in here.
	TRACE("\n\nWidth  : %d\nHeight : %d\nCodec ID : %d\nFPS : %f\nCodec : %s\n     ", m_VideoInfo.width, m_VideoInfo.height, m_VideoInfo.codec_id, m_VideoInfo.fps, m_VideoInfo.codecName.c_str());
}

bool CCamera::Impl::Open(const std::string& url)
{
	AVDictionary* options = nullptr;
	bool isRtsp = (url.rfind("rtsp://", 0) == 0);
	if (isRtsp){
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
		return false;

	// This tells FFmpeg to inspect the file (or stream) and discover its contents.
	if (avformat_find_stream_info(m_formatContext, nullptr) < 0)
		return false;

	// Find the first video stream in the file (or stream).
	for (unsigned int i = 0; i < m_formatContext->nb_streams; i++){
		AVStream* stream = m_formatContext->streams[i];
		if (stream->codecpar->codec_type == AVMEDIA_TYPE_VIDEO){
			m_videoStream = i;
			break;
		}
	}
	if (m_videoStream == -1)
		return false;

	// Find the decoder for the video stream.
	// If the video is H.264, decoder will point to FFmpeg's H.264 decoder, etc etc.
	AVCodecParameters* codecPar = m_formatContext->streams[m_videoStream]->codecpar;
	const AVCodec* decoder = avcodec_find_decoder(codecPar->codec_id);
	if (!decoder)
		return false;

	// Create a decoder context.
	// Copy codec parameters from the stream to the codec context.
	// Think of the AVCodecContext as the decoder instance that maintains all the state needed to decode the video.
	m_codecContext = avcodec_alloc_context3(decoder);
	if (!m_codecContext)
		return false;

	// Copy the stream parameters.
	// The stream contains information like - Width, Height, Pixel format, Codec profile. Copy all of that into the decoder context.
	if (avcodec_parameters_to_context(m_codecContext, codecPar) < 0)
		return false;

	// Open the decoder.
	if (avcodec_open2(m_codecContext, decoder, nullptr) < 0)
		return false;

	// At this point the decoder is ready.
	// Allocate the packet and frame, these are reused for every decoded frame.
	// The packet is used to hold the compressed data read from the stream, and the frame is used to hold the decompressed data after decoding.
	m_packet = av_packet_alloc();
	m_frame  = av_frame_alloc();
	if (!m_packet || !m_frame)
		return false;

	// When Open() returns true, the camera (or file) is completely ready to decode.
	return true;
}

bool CCamera::Open(const std::string& url)
{
	if (m_Impl->m_videoStream == -1){
		if (!m_Impl->Open(url)){
			Close();
			return false;
		}
		GetVideoInfo();
	}
	// When Open() returns true, the camera (or file) is completely ready to decode.
	return true;
}

const CFrame& CCamera::CurrentFrame() const
{
	return m_Impl->m_currentFrame;
}

/* allocate storage for it.
int size = av_image_get_buffer_size(AV_PIX_FMT_RGB24, codecPar->width, codecPar->height, 1);
m_Impl->m_rgbBuffer = new uint8_t[size];
// and attach it to the RGB frame.
av_image_fill_arrays(m_Impl->m_rgbFrame->data, m_Impl->m_rgbFrame->linesize, m_Impl->m_rgbBuffer, AV_PIX_FMT_RGB24, codecPar->width, codecPar->height, 1); */

bool CCamera::Grab()
{
	// For an MP4, reaching end-of-file means Grab() returns false.
	// For an RTSP stream, there is no end-of-file. Instead, av_read_frame() might fail temporarily because of a network hiccup.
	// To do:
	// make Grab() distinguish between:
	// End of file (for local files).
	// Temporary network errors (for RTSP), where it can retry instead of immediately giving up.
	// (I'll leave it until I actually see a stream interruption).

	while (av_read_frame(m_Impl->m_formatContext, m_Impl->m_packet) >= 0){
		bool Ok = false;
		if (m_Impl->m_packet->stream_index == m_Impl->m_videoStream)
			if (avcodec_send_packet(m_Impl->m_codecContext, m_Impl->m_packet) >= 0)
				Ok = true;

		av_packet_unref(m_Impl->m_packet);
		if (Ok){
			// _unref, so the frame is returned to an empty state before FFmpeg writes the next image into it..
			av_frame_unref(m_Impl->m_currentFrame.m_Impl->m_frame);
			if (avcodec_receive_frame(m_Impl->m_codecContext, m_Impl->m_currentFrame.m_Impl->m_frame) == 0)
				return true;
		}
	}
	return false;
}

/*####################################################################################################################################

In std::atomic, Read-Modify-Write (RMW) operations are atomic, indivisible operations that read a value from a memory location, modify it (via an operation), and write the new value back in a single step.
Unlike ordinary non-atomic increments (like x++) which compile down to separate machine instructions for loading, changing, and saving memory, an RMW operation guarantees that no other thread can interleave 
or modify the variable between the read and the write steps. Furthermore, RMW operations are unique because they are guaranteed to always operate on the absolute latest value in the modification order of 
that atomic variable, forcing write propagation across CPU cores. 
 
Use std::atomic for simple, independent variables like single flags or counters, and use std::mutex when you need to protect multiple variables, complex objects, or multi-step operations.

Memory Order TagBehavior When Applied to an RMW Operation :

memory_order_relaxed - Guarantees atomicity and the RMW latest-value rule, but adds no synchronization barriers for surrounding non-atomic memory variables.
memory_order_acquire - Prevents subsequent read/write operations in the local thread from being reordered before this RMW step.
memory_order_release - Prevents preceding read/write operations in the local thread from being reordered after this RMW step.
memory_order_acq_rel - Serves as both an acquire barrier (for the read portion) and a release barrier (for the write portion).
memory_order_seq_cst - Imposes acq_rel behaviors and enforces a single, globally uniform order of all sequentially consistent operations across all threads.

When to Use std::atomic
  Simple Flags and Counters: Best for scalar values like a boolean run-state flag or an integer counter. “Use std::atomic for POD types that can leverage CPU atomic instructions for efficiency, and std::mutex for non-POD types.”
  Lock-Free Performance: Avoids putting threads to sleep, reducing heavy OS-level scheduling overhead for basic operations.
  Low Contention: Great for high-frequency reads and writes on isolated variables where locking a full mutex would slow down the program.

When to Use std::mutex
  Multiple Variables: Essential when two or more variables must change together to keep your data in a valid state (maintaining invariants).
  Complex Logic: Required if your code block contains multiple instructions or conditions that must execute together without interruption from other threads.
  Non-Trivial Objects: Safer and more practical for heavy data structures like vectors, maps, or custom classes where atomic instructions are not supported or practical.

  lock()     - Locks the mutex. If the mutex is already locked by another thread, the calling thread blocks until the lock becomes available.
  unlock()   - Unlocks the mutex, allowing waiting threads to acquire it.
  try_lock() - Tries to lock the mutex without blocking. It returns true if the lock was successfully acquired, and false otherwise.

  Calling lock() and unlock() manually is dangerous because exceptions or early returns can cause the mutex to remain locked forever, leading to a deadlock. 
  Always use RAII wrappers like std::lock_guard or std::scoped_lock to automatically release the lock when the scope ends. 
  (eg, std::lock_guard<std::mutex> lock(m_SharedFrames[m_HeadIndex].mtx); )

//####################################################################################################################################*/

#ifdef CAM_DIRECT_TO_GUI
void CCameraThread::GetNextFrame()
{
	// This function is called in the consumer thread. Alternatively, the consumer can just call CameraThreadObject::m_RingBuf.AcquireRead()  
	// directly and then process the frame, in which case this function will not be used.
	CFrame *pBuf = m_RingBuf.AcquireRead();
	if (pBuf){
		// Callback for consumer specific code.
		if (m_GetNextFrameCallback)
			m_GetNextFrameCallback(pBuf, m_pCallbackParam);
		m_RingBuf.ReleaseRead();
	}
	else {
		// No frame available. The producer is slower than the consumer at the moment.
	}
}
#endif

bool CCameraThread::WriteNextFrame(const CFrame& frame, CImageConverter& converter)
{
	// This function is called in the producer thread after a new frame has been grabbed from the camera. 
	// It converts the frame to RGB and writes it to the ring buffer.
	CFrame *pBuf = m_RingBuf.AcquireWrite();
	if (pBuf){
		if (!converter.Convert(frame, *pBuf, true)){
			m_ErrorLog += "\n convert to rgb failed \n";
			return false;
		}
		m_RingBuf.ReleaseWrite();

#ifdef CAM_DIRECT_TO_GUI
		// Optional callback for consumer specific instructions now we have a new frame (eg, PostMessage 
		// to the GUI thread to call ReadNextFrame and update display).
		if (m_FrameReadyCallback)
			m_FrameReadyCallback(0, m_pCallbackParam);
#endif
	}
	else {
		// Frame dropped, ring buffer is full. The consumer is slower than the producer at the moment.
	}
	return true; 
}

void CCameraThread::Run()
{
	// To do - proper error reporting for when thread exits unexpectedly.

	CCamera Cam;
	if (!Cam.Open(m_CamURL)){
		m_ErrorLog += "\n Open failed \n";
	}
	else {
		// Open succeeded, now we can grab frames.
		m_RingBuf.Initialise();
		bool Ok = true;
		CImageConverter converter;
		while (m_RunCode.load(std::memory_order_relaxed)==RUNCODE_ALIVE && Ok){

			// Grab will naturally block waiting for the next frame.
			if (Cam.Grab()){
				const CFrame& frame = Cam.CurrentFrame();
				if (!WriteNextFrame(frame, converter))
					Ok = false;
			}
			else {
				// Lost stream?
				m_ErrorLog += "\n Grab failed \n";
				Ok = false;
				// alternatively, sleep for a while - std::this_thread::sleep_for(std::chrono::seconds(1));
				// and attempt reconnect.
			}
		}
	}
	m_RunCode.store(RUNCODE_DEAD, std::memory_order_release);
}

void CCameraThread::Start(const std::string &url)
{
	// Start the camera thread. This function is called from the consumer thread.
	if(m_RunCode.load(std::memory_order_relaxed) == RUNCODE_DEAD){
		m_CamURL = url;
		m_RunCode.store(RUNCODE_ALIVE, std::memory_order_relaxed);
		std::thread ct(&CCameraThread::Run, this);
		ct.detach();
	}
}

void CCameraThread::Terminate()
{
	// function will block until thread has stopped.
	if (m_RunCode.load(std::memory_order_relaxed) != RUNCODE_DEAD){
		// kill thread.
		m_RunCode.store(RUNCODE_KILL, std::memory_order_relaxed);
		// wait for thread to terminate.
		while (m_RunCode.load(std::memory_order_relaxed) != RUNCODE_DEAD)
			std::this_thread::sleep_for(std::chrono::milliseconds(250));
	}
}

//####################################################################################################################################

bool CImageProcessingThread::CopyFrame(CFrame *pSource, CImageMem *pDest)
{
	if (pDest->pBits == nullptr){
		pDest->Wd      = pSource->Width();
		pDest->Ht      = pSource->Height();
		pDest->Span    = pSource->LineSize();
		pDest->Planes  = pDest->Span / pDest->Wd;
		pDest->Padding = pDest->Span - (pDest->Wd * pDest->Planes);
		pDest->Size    = pDest->Span * pDest->Ht;

		if (pDest->Span != pDest->Wd*3){
			// for now I'm only allowing RGB24 with no padding at the end of each line. If the camera is returning a different format, then the 
			// CImageConverter class should not be converting to RGB24 (which it might well do at the mo?), it should preserve the original format.
			m_ErrorLog += "\n error - currently frame must be RGB24 with no padding at end of line \n";
			return false;
		}
		pDest->Allocate();

		// The image buffer allocated above is a wasteful copy of the image data, because the GDI wont except this buffer when creating the DIBSection, 
		// so the better option is to call CreateDIBSection (here) and use the DIBSection's buffer directly, but that would require GDI code in this class, which I 
		// want to avoid. Or a complicated messy callback - which i might implement in due course. But for now, just copy the data into the intermediate buffer.

		// Another way of looking at it is, the GUI should not be displaying the live image all the time, the display should be off most the time with just the image 
		// detection running in the background, and only when the user wants to see the image should the GUI be updated. In which case this intermediate buffer is not a big deal.
		
	//	if (m_InitialiseDIBCallback)
	//		m_InitialiseDIBCallback(pDest, pSource->Width(), pSource->Height(), m_pCallbackParam);
	}

	if (pDest->pBits){
		if (pDest->Wd!=pSource->Width() || pDest->Ht!=pSource->Height() || pDest->Span!=pSource->LineSize()){
			// Frame size changed, this won't happen, so its an error.
			m_ErrorLog += "\n Frame size changed \n";
			return false;
		}
		// Copy the RGB data from the frame into our (ring) buffer.
		memcpy(pDest->pBits, pSource->Data(), pDest->Size);
		return true;
	}
	m_ErrorLog += "\n Frame data is null \n";
	return false;
}

bool CImageProcessingThread::DoImageProcessing(CImageMem *pImg)
{
	// Image processing logic here, eg, object detection.
	// write results to a log file or database.

	for(int i=0; i<pImg->Ht; i++){
		uint8_t *pLine = pImg->pBits + (i * pImg->Span);
		for(int j=0; j<pImg->Wd; j++){
			uint8_t *r = &pLine[j*pImg->Planes + 0];
			uint8_t *g = &pLine[j*pImg->Planes + 1];
			uint8_t *b = &pLine[j*pImg->Planes + 2];
			
			// do something with r,g,b
			*r = 255 - *r;	// invert red channel
		//	*g = 255 - *g;	// invert green channel
		//	*b = 255 - *b;	// invert blue channel
		}
	}
	return true;
}

bool CImageProcessingThread::WriteNextFrame(CFrame *pRgbFrame)
{
	bool Ok = true;
	CImageMem *pIM = m_RingBuf.AcquireWrite();
	if (pIM){
		Ok = false;
		if (CopyFrame(pRgbFrame, pIM))
			if (DoImageProcessing(pIM))
				Ok = true;

		m_RingBuf.ReleaseWrite();
		// Optional callback for consumer specific instructions now we have processed a new frame (eg, PostMessage 
		// to the GUI thread to call ReadNextFrame and update display).
		if (m_FrameReadyCallback)
			m_FrameReadyCallback(0, m_pCallbackParam);
	}
	else {
		// Frame dropped, ring buffer is full. The consumer (GUI thread) is slower than the producer (this image processing thread) at the moment.
	}

	// so hang on, this producer should not (always) be waiting for the consumer (GUI thread) to read a frame and therefore free up a slot in the ring buffer,
	// because the GUI will not be displaying most the time and image processing will be a background task. So if the GUI display is off then we can call 
	// AcquireRead/ReleaseRead for every AcquireWrite/ReleaseWrite so that the ring buffer is always empty ready for the next frame from the camera thread.

/*	if (RunningInBackground){
		if (m_RingBuf.AcquireRead())
			m_RingBuf.ReleaseRead();
	}*/
	return Ok;
}

void CImageProcessingThread::GetNextFrame()
{
	// This function is called in the consumer thread. Alternatively, the consumer can just call CameraThreadObject::m_RingBuf.AcquireRead()  
	// directly and then process the frame, in which case this function will not be used.
	CImageMem *pBuf = m_RingBuf.AcquireRead();
	if (pBuf){
		// Callback for consumer specific code.
		if (m_GetNextFrameCallback)
			m_GetNextFrameCallback(pBuf, m_pCallbackParam);
		m_RingBuf.ReleaseRead();
	}
	else {
		// No frame available. The producer is slower than the consumer at the moment.
	}
}

void CImageProcessingThread::Run()
{
	// To do - proper error reporting for when thread exits unexpectedly.

	bool Ok = true;
	m_RingBuf.Initialise();
	while (m_RunCode.load(std::memory_order_relaxed)==RUNCODE_ALIVE && Ok){

		CFrame *pRgbFrame = m_pCamThread->m_RingBuf.AcquireRead(); // @@@@@ DONT LIKE THIS ???, I THINK THE CALLBACK METHOD SHOULD BE USED INSTEAD?
		if (pRgbFrame){
			Ok = WriteNextFrame(pRgbFrame);
			m_pCamThread->m_RingBuf.ReleaseRead();
		}
		else {
			// No frame available. The producer (camera thread) is slower than the consumer (this hread) at the moment.
			std::this_thread::sleep_for(std::chrono::milliseconds(25));
		}
	}
	m_RunCode.store(RUNCODE_DEAD, std::memory_order_release);
}

void CImageProcessingThread::Terminate()
{
	// function will block until thread has stopped.
	if (m_RunCode.load(std::memory_order_relaxed) != RUNCODE_DEAD){
		// kill thread.
		m_RunCode.store(RUNCODE_KILL, std::memory_order_relaxed);
		// wait for thread to terminate.
		while (m_RunCode.load(std::memory_order_relaxed) != RUNCODE_DEAD)
			std::this_thread::sleep_for(std::chrono::milliseconds(250));
	}
}

void CImageProcessingThread::Start( CCameraThread *pCamThread,
									void (*FrameReadyCallback)(int Code, void *pParam), 
									void (*GetNextFrameCallback)(const CImageMem *pImage, void *pParam), 
									void *pCallbackParam )
{
	// std::thread : https://en.cppreference.com/cpp/thread/thread/thread
	if (m_RunCode.load(std::memory_order_relaxed) == RUNCODE_DEAD){
		m_pCamThread = pCamThread;
		m_FrameReadyCallback = FrameReadyCallback;
		m_GetNextFrameCallback = GetNextFrameCallback;
		m_pCallbackParam = pCallbackParam;

		m_RunCode.store(RUNCODE_ALIVE, std::memory_order_relaxed);
		std::thread ipt(&CImageProcessingThread::Run, this); // ipt runs CImageProcessingThread::Run on this object 
		ipt.detach();
	}
}

