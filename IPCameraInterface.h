
#pragma once

#include "Camera.h"


class CMyBitmap
{
public:
	HBITMAP    m_hBitmap=nullptr;
	BITMAPINFO m_BMI={0};
	uint8_t	  *m_pData=nullptr;
	CDC        m_MemDC;	// virtual drawing surface in memory.

	CMyBitmap(){}
	~CMyBitmap(){ Delete(); }
	bool Create(CDC *pDC, int Wd, int Ht, int BitsPerPixel);
	void Delete();
};


// Windows/Linux - There is Windows specific code in here, and its unavoidable. I will need 
// to have windows and linux specific builds with sections of code 'ifdef'ed for each version.
#ifdef _WINDOWS_
#elif defined _LINUX_
#endif

#define IMGPROC_FRAMES	3
class CIPCameraInterface
{
	// ALSO - i can have multiple camera streams, one CIPCameraInterface per stream. - TRY THIS NEXT ?? 
	// ask chatGPT if FFmpeg will be ok with multiple CCamera instances - wondering if it might want to be initialised once and used for multiple cams ..hopefully not.
	// and new .h .cpp for this
public:
	bool Start(std::string CameraURL, HWND hWnd, UINT CameraReadyMsg, UINT FrameReadyMsg);
	void Shutdown();
	bool CameraReadyMessageHandler(int Wd, int Ht, CDC *pScreen);
	void FrameReadyMessageHandler(CDC *pScreen);

private:
	bool m_Init=0;
	HWND m_hWnd;
	UINT m_CameraReadyMsg, m_FrameReadyMsg;

	// camera thread data..
	CFrame                 m_Frame[3];
	SPSCRingBuffer<CFrame> m_CameraToProcessor;
	CCameraThread          m_CamThread;
	// image processing thread data..
	CMyBitmap                       m_GuiData[IMGPROC_FRAMES];
	PROCESSED_FRAME                 m_ProcessedFrame[IMGPROC_FRAMES];
	SPSCRingBuffer<PROCESSED_FRAME> m_ProcessorToGui;
	CImageProcessingThread          m_ImgProcThread;

	static void Static_CameraReadyCallback(int Wd, int Ht, void *pParam);
	static void Static_FrameReadyCallback(int Code, void *pParam);
};


/*
next TIME TO MOVE ON -

add some real image processing, first add boxes feedback from img proc to gui.
dont get bogged down on implementation, project is about learning new concepts, not spending ages implementing.
Eg, linux, or in Web UI.

*/

// image processing ideas...
//  - plate finder
//  - OpenCV for OCR, motion detection, and what else does it do?
//  - Darknet for object detection
//  - maybe Ollama and a LLM ( Gemma4 ).

//
// Web UI instead of (or as well as) linux version.
// Multiple cameras and multiple displays
// DVR functionality, my own Ip cam recording on pc/Linux 

