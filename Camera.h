
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
	int Stride() const;
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
	// Maybe pointless for my little project, but I wanted to try it out. It is a good technique for large projects where you want to hide implementation details and reduce compilation dependencies.
	class Impl; // forward declaration
	std::unique_ptr<Impl> m_Impl;   // hide impl details
	// The unique_ptr will automatically manage the memory of the implementation object, ensuring proper cleanup when the CImageConverter object is destroyed.
	// Also unique_ptr cannot be copied (can only be moved), which is consistent with the design of the CImageConverter class, which is not copyable.
};


// SPSCRingBufManager implements a SPSC (Single-Producer Single-Consumer) ring buffer. Because it strictly involves exactly one writer 
// and one reader, it eliminates the need for expensive mutex locks, making it wait-free and highly optimized for real-time systems.
// The buffers are not protected by a lock, instead, ownership of the buffer is being transferred between the two threads.
// m_Num is a convenient single synchronisation variable because it represents the ownership transfer:
// 
// Producer:  Frame written  ->  Advance head, NumFrames++  ->  Consumer can read
// Consumer:  Frame read     ->  Advance tail, NumFrames--  ->  Producer can reuse

template <int MAX_ENTRIES> class SPSCRingBufManager
{
private:
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

	bool AcquireRead()
	{	// if true - consumer can read entry at m_TailIndex ( GetReadIndex() ).
		return (m_Num.load(std::memory_order_acquire) > 0);
	}
	int GetReadIndex() const { return m_TailIndex; }
	void ReleaseRead()
	{
		m_TailIndex++;
		if (m_TailIndex == MAX_ENTRIES)	// ring buffer - wraparound back to start.
			m_TailIndex = 0;
		// Release the entry back to producer by subtracting 1 from m_Num.
		m_Num.fetch_sub(1, std::memory_order_release);
	}

	bool AcquireWrite()
	{	// if true - producer can write entry at m_HeadIndex ( GetWriteIndex() ).
		return (m_Num.load(std::memory_order_acquire) < MAX_ENTRIES);
	}
	int GetWriteIndex() const { return m_HeadIndex; }
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

#define MAX_FRAMES 4

class CCameraThread
{
public:
	void Run();
	void Terminate();		// called from consumer
	void GetNextFrame();	// called from consumer

	std::string m_ErrorLog, m_CamURL;
	void *m_pCallbackParam;

	
	
	// not gonna need this one here, it will be 
	// in the image processing thread class.
	// i suggest the img proc class just keeps polling/looping for frames, 
	// and when it gets one, it processes it and then calls the callback to the GUI thread to update the display.
	// 
	// if it doesnt get a frame it should just sleep for a few milliseconds 

	void (*m_FrameReadyCallback)(int, void*) = nullptr;		// [in producer] - post a message from producer to consumer to say a new frame is ready.
	



	void (*m_GetNextFrameCallback)(const CFrame& rgbFrame, void*) = nullptr;	// [in consumer] - process next frame buffer, eg, paint it on the window.

private:
	SPSCRingBufManager<MAX_FRAMES> m_Ring;
	CFrame m_SharedFrames[MAX_FRAMES];
	std::atomic<int> m_RunCode{RUNCODE_DEAD};	// producer/consumer shared.

	bool CacheSharedFrame(const CFrame& frame, CImageConverter& converter);
};


/*
class CImageProcessingThread
{
public:
	void Run();
	void Terminate();		// called from consumer
	void GetNextFrame();	// called from consumer

	std::string m_ErrorLog;
	void *m_pCallbackParam;

	void (*m_FrameReadyCallback)(int, void*) = nullptr;		// [in producer] - post a message from producer to consumer to say a new frame is ready.
	void (*m_GetNextFrameCallback)(const CFrame& rgbFrame, void*) = nullptr;	// [in consumer] - process next frame buffer, eg, paint it on the window.

private:
	SPSCRingBufManager<MAX_FRAMES> m_Ring;

	
	// ??? or just the image bits ???
	MEMORYDC m_SharedFrames[MAX_FRAMES];

	
	std::atomic<int> m_RunCode{RUNCODE_DEAD};	// producer/consumer shared.

	bool CacheSharedFrame(const CFrame& frame, CImageConverter& converter);
};
*/



