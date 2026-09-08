
#pragma once

#include <string>
#include <memory>
#include <thread>
#include "ImageProcessing.h"

class CFrame
{
public:
	CFrame();
	~CFrame();
	// These 2 lines tell the compiler: Do not generate a copy constructor or copy assignment operator 
	// for this class. If anyone tries to copy a CFrame object, it will result in a compile-time error.
	CFrame(const CFrame&) = delete;
	CFrame& operator=(const CFrame&) = delete;

	int Width() const;
	int Height() const;
	int LineSize() const;
	const uint8_t* ScanLine(int y) const;
	const uint8_t* Data() const;
	bool IsValid() const;
	int PixelFormat() const;
	const char *PixelFormatName() const;

private:
	class Impl;
	std::unique_ptr<Impl> m_Impl;

	friend class CCamera;
	friend class CImageConverter;
};


class CImageConverter
{
public:
	CImageConverter();
	~CImageConverter();
	// Non-copyable
	CImageConverter(const CImageConverter&) = delete;
	CImageConverter& operator=(const CImageConverter&) = delete;

	bool Convert(const CFrame& source, CFrame& destination, bool BGR=false);

private:
//	std::vector<std::string> m_Errors;	todo add error reporting

	class Impl;
	std::unique_ptr<Impl> m_Impl;
};


struct VIDEO_INFO
{
	bool StreamOpen = false;
	std::string codecName;
	int width = 0, height = 0;
	int codec_id = 0;
	double fps = 0.0;
	void Reset(){
		codecName.clear();
		StreamOpen = false;
		width = 0; height = 0;
		codec_id = 0;
		fps = 0.0;
	}
};


class CCamera
{
public:
	VIDEO_INFO m_VideoInfo;
	CCamera();
	~CCamera();
	// Non-copyable
	CCamera(const CCamera&) = delete;
	CCamera& operator=(const CCamera&) = delete;

	bool Open(const std::string& url);
	void Close();
	bool Grab();
	const CFrame& CurrentFrame() const;

private:
//	std::vector<std::string> m_Errors;	todo add error reporting

	void GetVideoInfo();
	// The PImpl Idiom (Pointer to IMPLementation) is a technique used for separating implementation from the interface. It minimizes header exposure.
	// It is a good technique for projects where you want to hide implementation details and reduce compilation dependencies.
	class Impl; // forward declaration
	std::unique_ptr<Impl> m_Impl;   // hide impl details
	// The unique_ptr will automatically manage the memory of the implementation object, ensuring proper cleanup when the CImageConverter object is destroyed.
	// Also unique_ptr cannot be copied (can only be moved), which is consistent with the design of the CImageConverter class, which is not copyable.
};


// CRingBuffer implements a SPSC (Single-Producer Single-Consumer) ring buffer. Because it strictly involves exactly one writer and one reader, it eliminates 
// the need for expensive mutex locks, making it wait-free and optimised for real-time systems. The buffers are not protected by a lock, instead, ownership of
// the buffer is being transferred between the two threads. m_Num is a convenient single synchronisation variable because it represents the ownership transfer:
// 
// Producer:  Frame written  ->  Advance head, NumFrames++  ->  Consumer can read
// Consumer:  Frame read     ->  Advance tail, NumFrames--  ->  Producer can reuse
template <typename Ty> class CRingBuffer
{
private:
	Ty *m_pBuf = nullptr;
	int m_Max = 0;
	int m_TailIndex = 0;		// consumer only
	int m_HeadIndex = 0;		// producer only
	std::atomic<int> m_Num{0};	// producer/consumer shared.
public:
	void AssignBuffer(Ty *pBuf, int NumEntries)
	{
		m_pBuf = pBuf;
		m_Max = NumEntries;
	}
	bool InitRing()
	{
		if (m_pBuf){
			m_HeadIndex = 0;	// Head = next index the producer will write.
			m_TailIndex = 0;	// Tail = next index the consumer will read.
			m_Num.store(0, std::memory_order_relaxed);	// m_Num = number of completed entries currently in the buffer.
			return true;
		}
		return false;
	}
	Ty *AcquireRead()
	{
		if (m_Num.load(std::memory_order_acquire) > 0)
			return &m_pBuf[m_TailIndex];	// consumer can read entry at m_TailIndex.
		return nullptr;
	}
	void ReleaseRead()
	{
		m_TailIndex++;
		if (m_TailIndex == m_Max)	// ring buffer - wraparound back to start.
			m_TailIndex = 0;
		// Release the entry back to producer by subtracting 1 from m_Num.
		m_Num.fetch_sub(1, std::memory_order_release);
	}
	Ty *AcquireWrite()
	{
		if (m_Num.load(std::memory_order_acquire) < m_Max)
			return &m_pBuf[m_HeadIndex];	// producer can write entry at m_HeadIndex.
		return nullptr;
	}
	void ReleaseWrite()
	{
		m_HeadIndex++;
		if (m_HeadIndex == m_Max)	// ring buffer - wraparound back to start.
			m_HeadIndex = 0;
		// Publish the completed entry by adding 1 to m_Num.
		m_Num.fetch_add(1, std::memory_order_release);
	}
};

enum class IMAGEFORMAT
{
	IMGFMT_RGB24 = 0,
	IMGFMT_BGR24,
	IMGFMT_GREY8,
	// ...
};

#define CAMERA_READY_CALLBACK_PARAMS	VIDEO_INFO &VI, void *pParam

class CCameraThread
{
public:
	CCameraThread(){}
	~CCameraThread(){ Terminate(); }
	// TO DO - caller needs to specify the required image format, eg, RGB 24bit, or greyscale 8bit.
	void Start(const std::string& url, IMAGEFORMAT Output, CRingBuffer<CFrame> *pRingBuffer, void (*CameraReadyCallback)(CAMERA_READY_CALLBACK_PARAMS), void *pCallbackParam);
	void Terminate();
	const std::vector<std::string>& GetErrorLog() const { return m_ErrorLog; }

private:
	std::vector<std::string> m_ErrorLog;
	CRingBuffer<CFrame> *m_pRingBuf=nullptr;

	std::thread       m_Thread;
	std::atomic<bool> m_StopRequested{false};
	std::string       m_CamURL;
	void             *m_pCallbackParam;
	IMAGEFORMAT       m_OutputFormat;
	CImageConverter   m_Converter;

	void Run();
	bool WriteNextFrame(const CFrame& frame);
	void (*m_CameraReadyCallback)(CAMERA_READY_CALLBACK_PARAMS) = nullptr;
};


class CFpsCounter
{
public:
	/* A nice thing about steady_clock, you don't have to worry about the units. You could measure something like:
	auto start = std::chrono::steady_clock::now();
	// do something
	auto elapsed = std::chrono::steady_clock::now() - start;
	and then explicitly ask for milliseconds:
	auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();  */
	void Init(size_t frameCount)
	{
		Reset();
		m_Timestamps.resize(frameCount);
		m_Initialized = true;
	}
	void Reset()
	{
		m_Index = 0;
		m_NumFrames = 0;
		m_FPS = 0.0;
		m_Initialized = false;
	}
	void Frame()
	{
		if (m_Initialized){
			const auto now = std::chrono::steady_clock::now();
			m_Timestamps[m_Index] = now;
			m_Index = (m_Index + 1) % m_Timestamps.size();

			if (m_NumFrames < m_Timestamps.size())
				++m_NumFrames;
			else {
				// only start calculating FPS after we have enough frames to fill the buffer, otherwise the FPS will be inaccurate.
				const size_t oldestIndex = (m_Index % m_Timestamps.size());
				const double seconds = std::chrono::duration<double>(now - m_Timestamps[oldestIndex]).count();
				if (seconds > 0.0)
					m_FPS = static_cast<double>(m_NumFrames - 1) / seconds;
			}
		}
	}
	double GetFPS() const
	{
		return m_FPS;
	}

private:
	std::vector<std::chrono::steady_clock::time_point> m_Timestamps; // circular buffer of timestamps for rolling average calculation.
	size_t m_Index = 0;
	size_t m_NumFrames = 0;
	double m_FPS = 0.0;
	bool   m_Initialized = false;
};


struct XY_POINT
{
	int x, y;
};

struct IMG_PROC_OUTPUT
{
//	std::vector<XY_POINT> Points;
	XY_POINT Pt[4];
	// ...
};

struct IMAGE_BUF
{
	// pData - this is the image buffer CImageProcessingThread::WriteNextFrame(CFrame *pRgbFrame) will write to. 
	// CImageProcessingThread is not responsible for this memory. It is up to the calling-thread/GUI code to allocate 
	// it at start up, which it might do using the win32 GDI functions, or just simply malloc/new.
	uint8_t	*pData;
	int      Wd, Ht;
	int      Planes, LineSize, Padding;
};

struct PROCESSED_FRAME
{
	void           *pGuiData=nullptr;	// pointer the real bitmap (see class CMyBitmap) managed by the GDI functions.
	IMAGE_BUF       Image={0};			// contains pointer to the raw bitmap bytes.
	IMG_PROC_OUTPUT ImgProcOut={0};		// data output from image processing.
	double          CurrentFPS=0.0;
};


#define FRAME_READY_CALLBACK_PARAMS int Code, void *pParam

class CImageProcessingThread
{
public:
	CImageProcessingThread(){}
	~CImageProcessingThread(){ Terminate(); }
	void Start(CRingBuffer<CFrame> *pInputRingBuffer, CRingBuffer<PROCESSED_FRAME> *pOutputRingBuffer, void (*FrameReadyCallback)(FRAME_READY_CALLBACK_PARAMS), void *pCallbackParam, int ThreadIndex);
	void Terminate();
	const std::vector<std::string>& GetErrorLog() const { return m_ErrorLog; }

private:
	CRingBuffer<CFrame>          *m_pInputRingBuf=nullptr;
	CRingBuffer<PROCESSED_FRAME> *m_pOutputRingBuf=nullptr;

	std::thread       m_Thread;
	std::atomic<bool> m_StopRequested{false};
	void             *m_pCallbackParam;
	int               m_ThreadIndex;
	bool              m_InitImageProcessing;
	CMotionDetector   m_MotionDetector;
	CFpsCounter       m_FpsCounter;
	
	std::vector<std::string> m_ErrorLog;
	std::chrono::steady_clock::time_point m_Timer;

	void Run();
	bool CopyFrame(CFrame *pSource, PROCESSED_FRAME *pDest);
	bool ImageProcessing(PROCESSED_FRAME *pFrame);
	bool WriteNextFrame(CFrame *pRgbFrame);
	void (*m_FrameReadyCallback)(FRAME_READY_CALLBACK_PARAMS) = nullptr;		// [in producer] - post a message from producer to consumer to say a new frame is ready.
};


// TO DO - 
// 
// @@@@@ inconsistant use of 'const' @@@@@ put it in member functions,  (WriteNextFrame for example)
// 
// inconsistant use of references, I'm using pointers in places where references would be better practice.
//
// Option to have the display on or off, if off then image processing should run in background.
//
/*
auto currentTime = std::chrono::high_resolution_clock::now();
double elapsed = std::chrono::duration<double>(currentTime - prevTime).count();
double fps = 1.0 / elapsed;
prevTime = currentTime;
*/





