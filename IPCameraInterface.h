
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


#define RING_BUF_SLOTS 3
class CIPCameraInterface
{
public:
	bool Start(const IPCAMERASETUP& Setup);//std::string CameraURL, HWND hWnd, uint32_t CameraReadyMsg, uint32_t FrameReadyMsg);
	void Shutdown();
	bool CameraReadyMessageHandler(int Wd, int Ht, CDC *pScreen);
	void FrameReadyMessageHandler(CDC *pScreen, float Scale, int X, int Y);	// pScreen, change name to pCDC ? ..because it might not yet be the screen.

private:
	bool m_Init=0;
	IPCAMERASETUP m_Setup;

	// camera thread data..
	CFrame                 m_Frame[RING_BUF_SLOTS];
	SPSCRingBuffer<CFrame> m_CameraToProcessor;
	CCameraThread          m_CamThread;
	// image processing thread data..
	CMyBitmap                       m_GuiData[RING_BUF_SLOTS];
	PROCESSED_FRAME                 m_ProcessedFrame[RING_BUF_SLOTS];
	SPSCRingBuffer<PROCESSED_FRAME> m_ProcessorToGui;
	CImageProcessingThread          m_ImgProcThread;

	static void Static_CameraReadyCallback(int Wd, int Ht, void *pParam);
	static void Static_FrameReadyCallback(int Code, void *pParam);
	void DrawImgProcOutput(CDC *pCDC, IMG_PROC_OUTPUT *pIPO);
};


class CIPCameraManager
{
private:
	CIPCameraInterface *m_pCams=nullptr;
	std::vector<IPCAMERASETUP> m_Setup;
	bool m_DisplayEnabled;

public:
	// rather than posting all cameras FrameReadyMsg messages to the GUI thread, maybe there should be another thread to 
	// to manage all cameras, and compile all the drawing from all camera to one drawing surface, then give this to the GUI.
	// Because if there are lots of cameras the windows message loop/queue is gonna be servicing a high quantity of messages.

	bool InitialiseSetup(const std::string& ConfigPath, HWND hWnd, uint32_t CameraReadyMsg, uint32_t FrameReadyMsg);
	bool StartStreams();
	void TerminateStreams(); // @@@@@@@@ fix in here

	bool CameraReadyMessageHandler(WPARAM wParam, LPARAM lParam, CDC *pScreen){
		// GUI thread message handler.
		int CameraIndex = (int)wParam;
		int Wd = LPARAM2_LO(lParam);
		int Ht = LPARAM2_HI(lParam);
		return m_pCams[CameraIndex].CameraReadyMessageHandler(Wd, Ht, pScreen);
	}

	void FrameReadyMessageHandler(WPARAM wParam, LPARAM lParam, CDC *pScreen, int WindowCX, int WindowCY){
		// GUI thread message handler.
		// 
		// gui/caller should pass in current window size, then calculations here can adjaust the camera display to fit.

		// @@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@ THIS NEXT

		int CameraIndex = (int)wParam;
		m_pCams[CameraIndex].FrameReadyMessageHandler(pScreen, 1.0f, 20, 20);
	}
};


/*
dont get bogged down on nice-ities, project is about learning new stuff, not spending ages implementing details

image processing
  - OpenCV for OCR, motion detection, and what else does it do?
  - Darknet for object detection
  - maybe Ollama and a LLM ( Gemma4 ).

linux, and/or Web UI.

Idea - have no GUI controls, instead have a config.txt which is edited by hand. This way all controls gonna work on Linux without replicating GUI code.
  Also dont use windows registry - cam IP's in config.txt.

Multiple cameras and multiple displays
DVR functionality, my own Ip cam recording on pc/Linux 
*/

