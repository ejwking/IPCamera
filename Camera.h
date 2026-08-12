
#pragma once

#include <string>
#include <memory>

/*
(@@@) If copying were allowed, this would compile:
CFrame a;
CFrame b = a;

But what should that mean?
Should it: Copy the AVFrame? Share the AVFrame? Duplicate the image buffer? Just copy the pointer?
There isn't an obvious correct answer.

Without = delete
The compiler would try to generate copy operations automatically.
Because CFrame contains a std::unique_ptr, the copy constructor is actually already deleted by the compiler.
So this wouldn't compile anyway.
So why did I write them? Simply to make the intention explicit.
When someone reads the class, they immediately know:
"A CFrame is a unique owner of an image."
Are they necessary? Strictly speaking...No. Because std::unique_ptr already prevents copying.
*/

class CFrame
{
public:
	CFrame();
	~CFrame();

	// (@@@) These 2 lines tell the compiler: Do not generate a copy constructor or copy assignment operator 
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
	const char * PixelFormatName() const;

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
	CImageConverter(const CImageConverter&) = delete;	// (@@@)
	CImageConverter& operator=(const CImageConverter&) = delete;
	bool Convert(const CFrame& source, CFrame& destination, bool BGR=false);

private:
	class Impl;
	std::unique_ptr<Impl> m_Impl;
};


struct VideoInfo
{
	std::string codecName;
	int width = 0, height = 0;
	int codec_id = 0;
	double fps = 0.0;
};


class CCamera
{
public:
	VideoInfo m_VideoInfo;

	CCamera();
	~CCamera();

	void GetVideoInfo();
	bool Open(const std::string& url);
	bool Grab();
	const CFrame& CurrentFrame() const;
	void Close();

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
template <typename Ty, int MAX_ENTRIES> class SPSCRingBuffer
{
private:
	Ty  m_Buf[MAX_ENTRIES];
	int m_TailIndex = 0;		// consumer only
	int m_HeadIndex = 0;		// producer only
	std::atomic<int> m_Num{0};	// producer/consumer shared.
public:
	void Initialise()
	{
		m_HeadIndex = 0;	// Head = next index the producer will write.
		m_TailIndex = 0;	// Tail = next index the consumer will read.
		m_Num.store(0, std::memory_order_relaxed);	// m_Num = number of completed entries currently in the buffer.
	}
	Ty *AcquireRead()
	{
		if (m_Num.load(std::memory_order_acquire) > 0)
			return &m_Buf[m_TailIndex];	// consumer can read entry at m_TailIndex.
		return nullptr;
	}
	void ReleaseRead()
	{
		m_TailIndex++;
		if (m_TailIndex == MAX_ENTRIES)	// ring buffer - wraparound back to start.
			m_TailIndex = 0;
		// Release the entry back to producer by subtracting 1 from m_Num.
		m_Num.fetch_sub(1, std::memory_order_release);
	}
	Ty *AcquireWrite()
	{
		if (m_Num.load(std::memory_order_acquire) < MAX_ENTRIES)
			return &m_Buf[m_HeadIndex];	// producer can write entry at m_HeadIndex.
		return nullptr;
	}
	void ReleaseWrite()
	{
		m_HeadIndex++;
		if (m_HeadIndex == MAX_ENTRIES)	// ring buffer - wraparound back to start.
			m_HeadIndex = 0;
		// Publish the completed entry by adding 1 to m_Num.
		m_Num.fetch_add(1, std::memory_order_release);
	}
};

enum CAMERATHERAD_RUNCODE{
	RUNCODE_DEAD = 0,
	RUNCODE_ALIVE,
	RUNCODE_KILL,
};

#define MAX_FRAMES 3

class CCameraThread
{
public:
	SPSCRingBuffer<CFrame, MAX_FRAMES> m_RingBuf;	// public for now. to do - make private and add a callback for CImageProcessingThread to read the next frame.

	void Start(const std::string& url);
	void Terminate();

#ifdef CAM_DIRECT_TO_GUI
	void *m_pCallbackParam;
	// Just for testing - The camera thread will write directly to the GUI window, bypassing the image processing thread.
	void GetNextFrame();	// called from consumer
	void (*m_FrameReadyCallback)(int Code, void *pParam) = nullptr;		// [in producer] - optional.
	void (*m_GetNextFrameCallback)(const CFrame *pRgbFrame, void *pParam) = nullptr;	// [in consumer] - optional.
#endif

private:
	std::atomic<int> m_RunCode{RUNCODE_DEAD};	// producer/consumer shared.
	std::string m_ErrorLog, m_CamURL;
	void Run();
	bool WriteNextFrame(const CFrame& frame, CImageConverter& converter);
};


class CImageMem
{
public:
	uint8_t	*pBits=nullptr;
	int      Wd=0, Ht=0, Planes=0, Span=0, Padding=0, Size=0;
	CImageMem(){}
	~CImageMem()
	{
		if(pBits)
			delete[] pBits;
		Size = 0;
	}
	void Allocate()
	{
		if (Size>0 && !pBits)
			pBits = new uint8_t[Size];
	}
};

class CImageProcessingThread
{
public:
	void Start(CCameraThread *pCamThread, void (*FrameReadyCallback)(int Code, void *pParam), void (*GetNextFrameCallback)(const CImageMem *pImage, void *pParam), void *pCallbackParam);
	void Terminate();
	void GetNextFrame();	// called from consumer thread and will invoke callback GetNextFrameCallback() to process the next frame buffer.

private:
	SPSCRingBuffer<CImageMem, MAX_FRAMES> m_RingBuf;

	std::atomic<int> m_RunCode{RUNCODE_DEAD};	// producer/consumer shared.
	CCameraThread   *m_pCamThread;
	std::string      m_ErrorLog;
	void            *m_pCallbackParam;

	void Run();
	bool CopyFrame(CFrame *pSource, CImageMem *pDest);
	bool DoImageProcessing(CImageMem *pImg);
	bool WriteNextFrame(CFrame *pRgbFrame);

	void (*m_FrameReadyCallback)(int Code, void *pParam) = nullptr;		// [in producer] - post a message from producer to consumer to say a new frame is ready.
	void (*m_GetNextFrameCallback)(const CImageMem *pImage, void *pParam) = nullptr;	// [in consumer] - process next frame buffer, eg, paint it on the window.
	//void (*m_InitialiseDIBCallback)(CImageMem*, int, int, void*) = nullptr;	// [in consumer] - optional.
};


// TO DO - 
// 
// check with chatGPT my thread management is correct and using best practice.
// Add a utilities .h/.cpp and move the UTF16 to UTF8 conversion functions there and other utility functions like TRACE, message box, etc.
// 
// inconsistant use of 'const'
// using pointers in places where references would be 'better' - in adverted commas.
//
// Option to have the display on or off, if off then image processing should run in background.
// Statistics for frame rate, frames dropped, etc. for both camera thread and image processing thread, and GUI display.
//
// put plate finding module in (but not in github).




