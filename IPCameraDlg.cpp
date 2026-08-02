// IPCameraDlg.cpp : implementation file
//

#include "pch.h"
#include "framework.h"
#include "IPCamera.h"
#include "IPCameraDlg.h"
#include "afxdialogex.h"
#include <thread>

#ifdef _DEBUG
#define new DEBUG_NEW
#endif


// CIPCameraDlg dialog

// "C:\\Users\\edwar\\Desktop\\lesser used media\\tiktok vids\\Download.mp4"
// "rtsp://username:password@192.168.0.31:554/stream1"
#define IP_CAMERA_URL _T("")
#define REG_SECTION	_T("IPCameraWnd")
#define WM_APP_FRAME_READY (WM_APP + 1)


CString Utf8(const std::string& s)
{
	return CString(CA2W(s.c_str(), CP_UTF8));
}

// Convert UTF-16 (std::wstring) to UTF-8 (std::string)
std::string Utf16ToUtf8(const std::wstring& utf16Str)
{
	if (utf16Str.empty()) return "";

	int sizeNeeded = WideCharToMultiByte(CP_UTF8, 0, &utf16Str[0], (int)utf16Str.size(), NULL, 0, NULL, NULL);
	std::string utf8Str(sizeNeeded, 0);
	WideCharToMultiByte(CP_UTF8, 0, &utf16Str[0], (int)utf16Str.size(), &utf8Str[0], sizeNeeded, NULL, NULL);

	return utf8Str;
}


CIPCameraDlg::CIPCameraDlg(CWnd* pParent /*=nullptr*/)
	: CDialogEx(IDD_IPCAMERA_DIALOG, pParent)
{
	m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);
}

void CIPCameraDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CIPCameraDlg, CDialogEx)
	ON_WM_PAINT()
	ON_WM_QUERYDRAGICON()
	ON_WM_DESTROY()
	ON_MESSAGE(WM_APP_FRAME_READY, &CIPCameraDlg::OnFrameReady)
	ON_BN_CLICKED(IDC_BTN_CONNECT, &CIPCameraDlg::OnBnClickedBtnConnect)
END_MESSAGE_MAP()


// CIPCameraDlg message handlers

BOOL CIPCameraDlg::OnInitDialog()
{
	CDialogEx::OnInitDialog();

	// Set the icon for this dialog.  The framework does this automatically
	//  when the application's main window is not a dialog
	SetIcon(m_hIcon, TRUE);			// Set big icon
	SetIcon(m_hIcon, FALSE);		// Set small icon

	// TODO: Add extra initialization here
	m_CameraURL = AfxGetApp()->GetProfileString(REG_SECTION, _T("ip_camera_url"), IP_CAMERA_URL);

	return TRUE;  // return TRUE  unless you set the focus to a control
}

void CIPCameraDlg::OnDestroy()
{
	CDialogEx::OnDestroy();
	// TODO: Add your message handler code here
	m_CamThread.Terminate();
	FreeBitmapObjects(&m_Pic);
	AfxGetApp()->WriteProfileString(REG_SECTION, _T("ip_camera_url"), m_CameraURL);
}

// If you add a minimize button to your dialog, you will need the code below
//  to draw the icon.  For MFC applications using the document/view model,
//  this is automatically done for you by the framework.

void CIPCameraDlg::OnPaint()
{
	if (IsIconic()){
		CPaintDC dc(this); // device context for painting

		SendMessage(WM_ICONERASEBKGND, reinterpret_cast<WPARAM>(dc.GetSafeHdc()), 0);

		// Center icon in client rectangle
		int cxIcon = GetSystemMetrics(SM_CXICON);
		int cyIcon = GetSystemMetrics(SM_CYICON);
		CRect rect;
		GetClientRect(&rect);
		int x = (rect.Width() - cxIcon + 1) / 2;
		int y = (rect.Height() - cyIcon + 1) / 2;

		// Draw the icon
		dc.DrawIcon(x, y, m_hIcon);
	}
	else{
		CDialogEx::OnPaint();

	//	static int ct=0;
	//	TRACE("\n ##### OnPaint %d #####  ", ct++);
	}
}

// The system calls this function to obtain the cursor to display while the user drags the minimized window.
HCURSOR CIPCameraDlg::OnQueryDragIcon()
{
	return static_cast<HCURSOR>(m_hIcon);
}

int CIPCameraDlg::InitDisplayDC(CDC *pDC, MEMORYDC *pMemDC, int Wd, int Ht)
{
	if (pMemDC->InitDC==0 || pMemDC->InitBitmap==0){

		memset(&pMemDC->bmi.bmiHeader, 0, sizeof(pMemDC->bmi.bmiHeader));
		pMemDC->bmi.bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
		pMemDC->bmi.bmiHeader.biWidth       = Wd;
		pMemDC->bmi.bmiHeader.biHeight      = -Ht;		// HARD CODED - negative height to create a top-down DIB
		pMemDC->bmi.bmiHeader.biPlanes      = 1;
		pMemDC->bmi.bmiHeader.biBitCount    = 24;		// HARD CODING,  FIX THIS
		pMemDC->bmi.bmiHeader.biCompression = BI_RGB;
		void *pBitmapBits;
		pMemDC->hBitmap = CreateDIBSection(0, &pMemDC->bmi, DIB_RGB_COLORS, &pBitmapBits, 0, 0);

		if (pMemDC->hBitmap){
			pMemDC->pBits = (uint8_t*)pBitmapBits;
			pMemDC->InitBitmap = 1;
			if (pMemDC->DC.CreateCompatibleDC(pDC)){
				pMemDC->DC.SelectObject(pMemDC->hBitmap);
				pMemDC->InitDC = 1;
			}
		}
	}
	if (pMemDC->InitDC && pMemDC->InitBitmap)
		return 1;
	return 0;
}

void CIPCameraDlg::FreeBitmapObjects(MEMORYDC *pMDC)
{
	if (pMDC->InitDC)
		pMDC->DC.DeleteDC();
	pMDC->InitDC = 0;
	if (pMDC->InitBitmap)
		DeleteObject(pMDC->hBitmap);
	pMDC->InitBitmap = 0;
}

LRESULT CIPCameraDlg::OnFrameReady(WPARAM wParam, LPARAM lParam)
{
	// This is called from the GUI thread (in response to a posted WM_APP_FRAME_READY message), and will 
	// result in a call to GetNextFrameCallback() which will update the display with the new frame.
	if (m_pDC == nullptr)
		m_pDC = this->GetDC();
	m_CamThread.GetNextFrame();
	return 0;
}

void GetNextFrameCallback(const CFrame& rgbFrame, void *pParam)
{
	if (pParam){
		CIPCameraDlg *pDlg = (CIPCameraDlg*)pParam;

		if (pDlg->m_pDC){
			if (pDlg->InitDisplayDC(pDlg->m_pDC, &pDlg->m_Pic, rgbFrame.Width(), rgbFrame.Height())){

				memcpy(pDlg->m_Pic.pBits, rgbFrame.Data(), rgbFrame.Stride() * rgbFrame.Height());

				// Here we might do image processing on the rgbFrame, or we might do it in the camera thread before calling this callback.
				// If we do it here, we might want to do it in a separate thread so that the GUI thread isn't blocked for too long.
				// 
				// best option - third thread for image processing - maybe a thread pool for multiple frames in parallel.
				// 
				// do additional drawing on m_Pic.DC here if we want to overlay text or graphics on the video frame.

				pDlg->m_pDC->BitBlt(0, 0, rgbFrame.Width(), rgbFrame.Height(), &pDlg->m_Pic.DC, 0, 0, SRCCOPY);

				// alternatively, call InvalidateRect(...) and do the painting in OnPaint().
			}
		}
	}
}

void FrameReadyCallback(int Code, void *pParam)
{
	// This is called from the camera thread, not the GUI thread. 
	if (pParam){
		TRACE("\n    FrameReadyCallback %d  ", Code);
		CIPCameraDlg *pDlg = (CIPCameraDlg*)pParam;

		// Post message to the GUI thread.
		pDlg->PostMessage(WM_APP_FRAME_READY, 0, 0);
	}
}

void CIPCameraDlg::OnBnClickedBtnConnect()
{
	static int once=0;
	if (once == 0){
		
		once = 1; // need proper one-instance protection in CCameraThread class ?

		m_CamThread.m_CamURL = Utf16ToUtf8(m_CameraURL.GetString());
		m_CamThread.m_pCallbackParam = (void*)this;		// check with chatGPT
		m_CamThread.m_FrameReadyCallback = FrameReadyCallback;
		m_CamThread.m_GetNextFrameCallback = GetNextFrameCallback;

		// this should and could all be hidden in camera thread class function ?

		// std::thread : https://en.cppreference.com/cpp/thread/thread/thread

		std::thread ct(&CCameraThread::Run, &m_CamThread); // ct runs CCameraThread::Run on object m_CamThread
		ct.detach();
		// ct.join(); // Wait for the thread to finish before continuing.
	}
}

void CIPCameraDlg::RgbFrameDrawTest(const CFrame& rgbFrame)
{
	for (int y = 0; y < rgbFrame.Height(); y++){
		const uint8_t* scanline = rgbFrame.ScanLine(y);
		for (int x = 0; x < rgbFrame.Width(); x++){
			uint8_t r = scanline[x * 3 + 0];
			uint8_t g = scanline[x * 3 + 1];
			uint8_t b = scanline[x * 3 + 2];
			m_pDC->SetPixel(x,y,RGB(r,g,b));
		}
	}
}

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Separate thread for camera library.
// 
// Add image processing - ideas...
//  - plate finder
//  - OpenCV for OCR, motion detection, and what else does it do?
//  - Darknet for object detection
//  - maybe Ollama and a LLM ( Gemma4 ).
// 
// another thread for image processing.
// 
// Put camera library and image processing in a DLL so that the GUI can be replaced with another GUI framework or web interface.
// 
// LINUX version to run on my RPi.
// GUI for linux - wxWidgets. Or a web interface.
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

