
#pragma once

#include "Camera.h"
#include "utilities.h"


#ifdef _WINDOWS_
// Windows/Linux - There is Windows specific code in here, and its unavoidable. I will need to have windows and linux specific builds with sections of code 'ifdef'ed for each version.
// @@@@@@@ BUT, avoid MFC and just use plain win32 API. Easy to do, the (GDI) functions are the same.
#elif defined _LINUX_
// 
#endif

//typedef GUIMSG int;

class CMyBitmap
{
public:
	HBITMAP    m_hBitmap=nullptr;
	BITMAPINFO m_BMI={0};
	uint8_t	  *m_pData=nullptr;
	CDC        m_MemDC;	// virtual drawing surface in memory.

	CMyBitmap(){}
	~CMyBitmap(){ Delete(); }
	bool Create(CDC *pDC, int Wd, int Ht, int BitsPerPixel, bool TopDown);
	void Delete();
	bool IsCreated(){ return (m_hBitmap != nullptr); }
};


struct IPCAMERASETUP
{
	std::string Url;
	HWND hWnd;
	uint32_t CameraReadyMsg, FrameReadyMsg;
	uint32_t MessageSubCode;
};


#define RING_BUF_SLOTS 3 // ..or 4? 3 seems fine.

class CIPCameraInterface
{
public:
	bool Start(const IPCAMERASETUP& Setup);
	void Shutdown();
	bool CameraReadyMessageHandler(CDC *pScreen);
	void FrameReadyMessageHandler(CDC *pScreen, float Scale, int X, int Y);	// pScreen, change name to pCDC ? ..because it might not yet be the screen.

private:
	bool          m_Init=0;
	IPCAMERASETUP m_Setup;
	VIDEO_INFO    m_VideoInfo;	// This is accessed in the camera thread (once, for write) and gui thread (for read), it is not protected because access is asynchronous.

	// camera thread data..
	CFrame                 m_Frame[RING_BUF_SLOTS];
	SPSCRingBuffer<CFrame> m_CameraToProcessor;
	CCameraThread          m_CamThread;
	// image processing thread data..
	CMyBitmap                       m_GuiData[RING_BUF_SLOTS];
	PROCESSED_FRAME                 m_ProcessedFrame[RING_BUF_SLOTS];
	SPSCRingBuffer<PROCESSED_FRAME> m_ProcessorToGui;
	CImageProcessingThread          m_ImgProcThread;

	static void Static_CameraReadyCallback(CAMERA_READY_CALLBACK_PARAMS);
	static void Static_FrameReadyCallback(FRAME_READY_CALLBACK_PARAMS);
	void DrawImgProcOutput(CDC *pCDC, IMG_PROC_OUTPUT *pIPO);
};


// to do - put CIPCameraManager in a separate .h .cpp ?
#define SAFE_MAX_CAMERAS 16

class CIPCameraManager
{
private:
	// to do  
	// std::vector<std::string> m_Errors;
	// , also change my other error handling to a std::vector<std::string>

	CIPCameraInterface *m_pCams=nullptr;
	std::vector<IPCAMERASETUP> m_CamSetup;
	bool m_DisplayEnabled;

public:
	CIPCameraManager();
	~CIPCameraManager();

	bool InitialiseSetup(const std::string& ConfigPath, HWND hWnd, uint32_t CameraReadyMsg, uint32_t FrameReadyMsg);
	bool StartStreams();
	void TerminateStreams();
	bool CameraReadyMessageHandler(WPARAM wParam, LPARAM lParam, CDC *pScreen);
	void FrameReadyMessageHandler(WPARAM wParam, LPARAM lParam, CDC *pScreen, int WindowCX, int WindowCY);
};


/*
dont get bogged down on nice-ities, project is about learning new stuff, not spending ages implementing details

image processing
  - OpenCV for OCR, motion detection, and what else does it do?
  - Darknet for object detection
  - maybe Ollama and a LLM ( Gemma4 ).

linux, and/or Web UI.

DVR functionality, my own Ip cam recording on pc/Linux 
*/

