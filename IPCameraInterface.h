
#pragma once

#include "Camera.h"
#include "utilities.h"


#ifdef _WINDOWS_
// Windows/Linux - There is Windows specific code in here, and its unavoidable. I will need to have windows and linux specific builds with sections of code 'ifdef'ed for each version.
// @@@@@@@ BUT, avoid MFC and just use plain win32 API. Easy to do, the (GDI) functions are the same.
#elif defined _LINUX_
// 
#endif

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


#define RING_BUF_SLOTS 3
class CIPCameraInterface
{
public:
	bool Start(std::string CameraURL, HWND hWnd, UINT CameraReadyMsg, UINT FrameReadyMsg);
	void Shutdown();
	bool CameraReadyMessageHandler(int Wd, int Ht, CDC *pScreen);
	void FrameReadyMessageHandler(CDC *pScreen, float Scale, int X, int Y);	// pScreen, change name to pCDC ? ..because it might not yet be the screen.

private:
	bool m_Init=0;
	HWND m_hWnd;
	UINT m_CameraReadyMsg, m_FrameReadyMsg;

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


struct IPCAMERASETUP
{
	std::string Url;
	UINT CameraReadyMsg, FrameReadyMsg; // to do
	int X, Y; // drawing position.
	float Scale; // drawing scale.
};

class CIPCameraAppSetup
{
public:
	IPCAMERASETUP m_Camera[6];
	int m_NumCams=0;
	bool m_DisplayEnabled;

	void ReadConfigFile()
	{
		CConfigFile cfg;
		bool ok = cfg.load("C:\\EKING\\Projects\\C++\\IPCamera\\my notes\\config.txt");	// @@@@ hard coded for now.
		if (!cfg.getErrors().empty())
			for (auto& err : cfg.getErrors())
				TRACE(_T("\n config.txt error - %s "), Utf8(err.c_str()));

		m_DisplayEnabled = cfg.getBool("display_enabled");
		m_NumCams = 0;
		int Max_Cams = num_entries(m_Camera);
		for (int i=0; i<Max_Cams; i++){
			std::string name = "camera_url_" + std::to_string(i+1);
			m_Camera[i].Url = cfg.getString(name, "");
			if (!m_Camera[i].Url.empty())
				m_NumCams++;
		}
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

