
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
	CDC        m_MemDC;	// Virtual drawing surface in memory, with the dimensions of our (rgb24 converted) camera frame.

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
	VIDEO_INFO m_VideoInfo;	// This is accessed once in the camera thread (for write) and later in the gui thread (for read), it is not protected because access is synchronous.

	bool Start(const IPCAMERASETUP& Setup);
	void Shutdown();
	bool CameraReadyMessageHandler(CDC *pScreen);
	void FrameReadyMessageHandler(CDC *pScreen, int Cell_Left, int Cell_Top, int Cell_Wd, int Cell_Ht);

private:
	bool          m_Init=0;
	IPCAMERASETUP m_Setup;

	// camera thread data..
	CFrame                 m_Frame[RING_BUF_SLOTS];
	SPSCRingBuffer<CFrame> m_CameraToProcessor;
	CCameraThread          m_CamThread;
	
	// image processing thread data..
	PROCESSED_FRAME                 m_ProcessedFrame[RING_BUF_SLOTS];
	CMyBitmap                       m_GuiData[num_entries(m_ProcessedFrame)];
	SPSCRingBuffer<PROCESSED_FRAME> m_ProcessorToGui;
	CImageProcessingThread          m_ImgProcThread;

	static void Static_CameraReadyCallback(CAMERA_READY_CALLBACK_PARAMS);
	static void Static_FrameReadyCallback(FRAME_READY_CALLBACK_PARAMS);
	void DrawImgProcOutput(CDC *pCDC, IMG_PROC_OUTPUT *pIPO);
};


class CGridLayout
{
public:
	void Initialse(int rows, int cols, int innerPadding, int outerPadding)
	{
		m_rows = rows;
		m_cols = cols;
		m_inner = innerPadding;
		m_outer = outerPadding;
	}

	struct CellRect
	{
		int left, top, right, bottom;
	};

	CellRect GetCellRect(int windowWidth, int windowHeight, int row, int col)
	{
		m_width = windowWidth;
		m_height = windowHeight;
		if (row < 0 || row >= m_rows || col < 0 || col >= m_cols){
			// ("Cell index out of range");
			return { 0, 0, 0, 0 };
		}
		// Effective drawable area after outer padding
		int usableWidth  = m_width  - 2 * m_outer;
		int usableHeight = m_height - 2 * m_outer;
		int cellWidth  = usableWidth  / m_cols;
		int cellHeight = usableHeight / m_rows;
		int left   = m_outer + col * cellWidth  + m_inner;
		int top    = m_outer + row * cellHeight + m_inner;
		int right  = m_outer + (col + 1) * cellWidth  - m_inner;
		int bottom = m_outer + (row + 1) * cellHeight - m_inner;
		return { left, top, right, bottom };
	}

	CellRect GetCellRect(int windowWidth, int windowHeight, int index)
	{
		return GetCellRect(windowWidth, windowHeight, index/m_cols, index%m_cols);
	}
private:
	int m_width=0, m_height=0;
	int m_rows=0, m_cols=0;
	int m_inner;   // padding inside each cell
	int m_outer;   // padding around the grid
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
	CGridLayout m_Grid;

	void InitGridLayout();

public:
	CIPCameraManager();
	~CIPCameraManager();

	bool InitialiseSetup(const std::string& ConfigPath, HWND hWnd, uint32_t CameraReadyMsg, uint32_t FrameReadyMsg);
	bool StartStreams();
	void TerminateStreams();
	bool CameraReadyMessageHandler(WPARAM wParam, LPARAM lParam, CDC *pScreen);
	void FrameReadyMessageHandler(WPARAM wParam, LPARAM lParam, CDC *pScreen, int WindowCX, int WindowCY);
	int  DrawGridLayout(CDC *pScreen, int WindowCX, int WindowCY);
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

