// IPCameraDlg.cpp : implementation file
//

#include "pch.h"
#include "framework.h"
#include "IPCamera.h"
#include "IPCameraDlg.h"
#include "afxdialogex.h"
#include "utilities.h"


#ifdef _DEBUG
#define new DEBUG_NEW
#endif


// CIPCameraDlg dialog

// "C:\\Users\\edwar\\Desktop\\lesser used media\\tiktok vids\\Download.mp4"
// "rtsp://username:password@192.168.0.31:554/stream1"

#define IP_CAMERA_URL _T("")
#define REG_SECTION	_T("IPCameraWnd")
#define WM_APP_CAMERA_READY	(WM_APP + 1)
#define WM_APP_FRAME_READY	(WM_APP + 2)


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
	ON_MESSAGE(WM_APP_CAMERA_READY, &CIPCameraDlg::OnCameraReady)
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
	m_ImgProcThread.Terminate();
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
		FreeBitmapObjects(pMemDC);
		memset(&pMemDC->bmi.bmiHeader, 0, sizeof(pMemDC->bmi.bmiHeader));
		pMemDC->bmi.bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
		pMemDC->bmi.bmiHeader.biWidth       = Wd;
		pMemDC->bmi.bmiHeader.biHeight      = -Ht;	// HARD CODED (HACK??) - negative height to create a top-down DIB
		pMemDC->bmi.bmiHeader.biPlanes      = 1;	// ???
		pMemDC->bmi.bmiHeader.biBitCount    = 24;	// HARD CODED, FIX THIS
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

	if (pMemDC->InitDC && pMemDC->InitBitmap){
		if (pMemDC->bmi.bmiHeader.biWidth==Wd && abs(pMemDC->bmi.bmiHeader.biHeight)==Ht)
			return 1;
		// bitmap size has changed, I dont want to cope with this because it shouldn't happen - and if i did we would not want to keep deleting/creating bitmaps.
		::AfxMessageBox(_T("error - Bitmap size has changed."));
	}
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
	// WM_APP_FRAME_READY message handler.
	if (m_pDC == nullptr)
		m_pDC = this->GetDC();

	CImageMem *pBuf = m_ProcessorToGui.AcquireRead();
	if (pBuf){
		if (m_pDC){
			if (InitDisplayDC(m_pDC, &m_Pic, pBuf->Wd, pBuf->Ht)){
				memcpy(m_Pic.pBits, pBuf->pBits, pBuf->Size);
				m_pDC->BitBlt(0, 0, pBuf->Wd, pBuf->Ht, &m_Pic.DC, 0, 0, SRCCOPY);
				// alternatively, call InvalidateRect(...) and do the painting in OnPaint().
			}
		}
		m_ProcessorToGui.ReleaseRead();
	}
	else {
		// No frame available.
		// ...which shouldnt usually be the case because this message handler is only called after the FrameReadyCallback posts a WM_APP_FRAME_READY.
		// actually think about it, if WM_APP_FRAME_READY messages were not processed immediately and queued up, then m_ProcessorToGui would get full
		// and start dropping frames. Then when this side catches up and processes all the WM_APP_FRAME_READY messages, some will be empty (no frame)
		// due to the dropped frames - thats all fine.
	}
	return 0;
}

void FrameReadyCallback(int Code, void *pParam)
{
	// This is called in the image processing thread, not in the GUI thread. 
	if (pParam){
		CIPCameraDlg *pDlg = (CIPCameraDlg*)pParam;
		// Post message - non-blocking will put in receiving window (GUI thread) message queue.
		pDlg->PostMessage(WM_APP_FRAME_READY, 0, 0);
	}
}

LRESULT CIPCameraDlg::OnCameraReady(WPARAM wParam, LPARAM lParam)
{
	// WM_APP_CAMERA_READY message handler.

	if (m_pDC == nullptr)
		m_pDC = this->GetDC();

	int Wd = (int)wParam;
	int Ht = (int)lParam;

	// LineSize = image format (RGB 24, greyscale, ect) must be specified when opening the camera, that is how we will know it here.
	// HARD CODED FOR NOW.
	int LineSize = Wd * 3;

	for (int i=0; i<num_entries(m_ImageMem); i++){

		CImageMem *pI = &m_ImageMem[i];

		pI->Wd      = Wd;
		pI->Ht      = Ht;
		pI->Span    = LineSize;
		pI->Planes  = pI->Span / pI->Wd;
		pI->Padding = pI->Span - (pI->Wd * pI->Planes);
		pI->Size    = pI->Span * pI->Ht;

		pI->Allocate();

		// to do next, replace this allocate with buffer created here with CreateDIBSection.

	/*	
		MEMORYDC *pMdc = new MEMORYDC;

		if (InitDisplayDC(m_pDC, pMdc, Wd, Ht)){ // m_pDC - why we passing a member @@@@
		
			pI->pGuiData = (void*)pMdc;
			pI->pBits = pMdc->pBits;
		}

		// cleanup, ideally invoked by the CImageMem destructor. otherwise loadsa manual cleanup calls becomes a liability.
		// 1. FreeBitmapObjects
		// 2. delete pGuiData, and null pBits, and zero wd ht.
		*/


		// The image buffer allocated above is a wasteful copy of the image data, because the GDI wont except this buffer when creating the DIBSection, 
		// so the better option is to call CreateDIBSection (here) and use the DIBSection's buffer directly

		// Another way of looking at it is, the GUI should not be displaying the live image all the time, the display should be off most the time with just the image 
		// detection running in the background, and only when the user wants to see the image should the GUI be updated. In which case this intermediate buffer is not a big deal.
	}

	m_ProcessorToGui.AssignBuffer(m_ImageMem, num_entries(m_ImageMem));
	m_ImgProcThread.Start(&m_CameraToProcessor, &m_ProcessorToGui, FrameReadyCallback, (void*)this);

	return 0;
}

void CameraReadyCallback(int Wd, int Ht, void *pParam)
{
	// This is called in the camera thread, not in the GUI thread. 
	if (pParam){
		TRACE("\n  CameraReadyCallback %d x %d  ", Wd, Ht);
		CIPCameraDlg *pDlg = (CIPCameraDlg*)pParam;

		// Send message - blocks execution until the receiving window (GUI thread) processes the message.
		// pDlg->SendMessage(WM_APP_CAMERA_READY, (WPARAM)Wd, (LPARAM)Ht);

		// Using SendMessage is problematic here because, here is the scenario :
		// Camera thread starts. Calls this callback but at that instant the window in closed, and m_CamThread.Terminate() is called in OnDestroy.
		// Next, m_CamThread.Terminate() blocks the GUI thread until the camera thread terminates.
		// But the camera thread cant terminate because SendMessage won't return until WM_APP_CAMERA_READY msg is processed.
		// Conclusion - dont use SendMessage.
		pDlg->PostMessage(WM_APP_CAMERA_READY, (WPARAM)Wd, (LPARAM)Ht);
	}
}

void CIPCameraDlg::OnBnClickedBtnConnect()
{
	static int once=0;
	if (once++ == 0){
		m_CameraToProcessor.AssignBuffer(m_Frame, num_entries(m_Frame));
		// to do - specify required image format (RGB 24, greyscale, ect).
		m_CamThread.Start(Utf16ToUtf8(m_CameraURL.GetString()), &m_CameraToProcessor, CameraReadyCallback, (void*)this);
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

// image processing ideas...
//  - plate finder
//  - OpenCV for OCR, motion detection, and what else does it do?
//  - Darknet for object detection
//  - maybe Ollama and a LLM ( Gemma4 ).
