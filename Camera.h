
#pragma once

#include <string>
#include <memory>
#include <thread>


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
	class Impl;
	std::unique_ptr<Impl> m_Impl;
};


struct VIDEO_INFO
{
	std::string codecName;
	int width = 0, height = 0;
	int codec_id = 0;
	double fps = 0.0;
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
	void GetVideoInfo();
	bool Grab();
	const CFrame& CurrentFrame() const;

private:
	// The PImpl Idiom (Pointer to IMPLementation) is a technique used for separating implementation from the interface. It minimizes header exposure.
	// It is a good technique for projects where you want to hide implementation details and reduce compilation dependencies.
	class Impl; // forward declaration
	std::unique_ptr<Impl> m_Impl;   // hide impl details
	// The unique_ptr will automatically manage the memory of the implementation object, ensuring proper cleanup when the CImageConverter object is destroyed.
	// Also unique_ptr cannot be copied (can only be moved), which is consistent with the design of the CImageConverter class, which is not copyable.
};


// SPSCRingBuffer implements a SPSC (Single-Producer Single-Consumer) ring buffer. Because it strictly involves exactly one writer and one reader, it eliminates 
// the need for expensive mutex locks, making it wait-free and optimised for real-time systems. The buffers are not protected by a lock, instead, ownership of
// the buffer is being transferred between the two threads. m_Num is a convenient single synchronisation variable because it represents the ownership transfer:
// 
// Producer:  Frame written  ->  Advance head, NumFrames++  ->  Consumer can read
// Consumer:  Frame read     ->  Advance tail, NumFrames--  ->  Producer can reuse
template <typename Ty> class SPSCRingBuffer
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

struct XY_POINT
{
	int x, y;
};

struct IMG_PROC_OUTPUT
{
	XY_POINT Pt[4];
	// ...
};

struct PROCESSED_FRAME
{
	// pData - this is the image buffer CImageProcessingThread::WriteNextFrame(CFrame *pRgbFrame) will write to. 
	// CImageProcessingThread is not responsible for this memory. It is up to the calling-thread/GUI code to allocate 
	// it at start up, which it might do using the win32 GDI functions, or just simply malloc/new.
	uint8_t	*pData=nullptr;
	int      Wd=0, Ht=0;
	int      Planes=0, Span=0, Padding=0;
	// Data from image processing..
	IMG_PROC_OUTPUT ImgProcOut;
	void *pGuiData=nullptr;
};

enum IMAGEFORMAT
{
	IMGFMT_RGB24 = 0,
	IMGFMT_GREY8,
};

#define CAMERA_READY_PARAMS	int Wd, int Ht, void *pParam

class CCameraThread
{
public:
	CCameraThread(){}
	~CCameraThread(){ Terminate(); }
	// TO DO - caller needs to specify the required image format, eg, RGB 24bit, or greyscale 8bit.
	void Start(const std::string& url, IMAGEFORMAT Output, SPSCRingBuffer<CFrame> *pRingBuffer, void (*CameraReadyCallback)(CAMERA_READY_PARAMS), void *pCallbackParam);
	void Terminate();

private:
	SPSCRingBuffer<CFrame> *m_pRingBuf=nullptr;

	std::thread       m_Thread;
	std::atomic<bool> m_StopRequested{false};
	std::string       m_ErrorLog, m_CamURL;
	void             *m_pCallbackParam;
	IMAGEFORMAT       m_OutputFormat;
	CImageConverter   m_Converter;

	void Run();
	bool WriteNextFrame(const CFrame& frame);
	void (*m_CameraReadyCallback)(CAMERA_READY_PARAMS) = nullptr;
};

#define FRAME_READY_PARAMS int Code, void *pParam

class CImageProcessingThread
{
public:
	CImageProcessingThread(){}
	~CImageProcessingThread(){ Terminate(); }
	void Start(SPSCRingBuffer<CFrame> *pInputRingBuffer, SPSCRingBuffer<PROCESSED_FRAME> *pOutputRingBuffer, void (*FrameReadyCallback)(FRAME_READY_PARAMS), void *pCallbackParam);
	void Terminate();

private:
	SPSCRingBuffer<CFrame>          *m_pInputRingBuf=nullptr;
	SPSCRingBuffer<PROCESSED_FRAME> *m_pOutputRingBuf=nullptr;

	std::thread       m_Thread;
	std::atomic<bool> m_StopRequested{false};
	std::string       m_ErrorLog;
	void             *m_pCallbackParam;

	void Run();
	bool CopyFrame(CFrame *pSource, PROCESSED_FRAME *pDest);
	bool DoImageProcessing(PROCESSED_FRAME *pFrame);
	bool WriteNextFrame(CFrame *pRgbFrame);
	void (*m_FrameReadyCallback)(FRAME_READY_PARAMS) = nullptr;		// [in producer] - post a message from producer to consumer to say a new frame is ready.
};


// TO DO - 
// 
// @@@@@ inconsistant use of 'const' @@@@@ put it in member functions,  (WriteNextFrame for example)
// 
// inconsistant use of references, I'm using pointers in places where references would be 'better' practice. Get used to using references where ever possible.
//
// Option to have the display on or off, if off then image processing should run in background.
// 
// Statistics for frame rate, frames dropped, etc. for both camera thread and image processing thread, and GUI display.





