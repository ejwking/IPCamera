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
		if (m_pDC == nullptr)
			m_pDC = this->GetDC();
	//	static int ct=0;
	//	TRACE("\n ##### OnPaint %d #####  ", ct++);
	}
}

// The system calls this function to obtain the cursor to display while the user drags the minimized window.
HCURSOR CIPCameraDlg::OnQueryDragIcon()
{
	return static_cast<HCURSOR>(m_hIcon);
}

bool CMyBitmap::Create(CDC *pDC, int Wd, int Ht, int BitsPerPixel)
{
	if (m_hBitmap){
		::AfxMessageBox(_T("error - CMyBitmap. Bitmap already created "));
	}
	else {
		memset(&m_BMI.bmiHeader, 0, sizeof(m_BMI.bmiHeader));
		m_BMI.bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
		m_BMI.bmiHeader.biWidth       = Wd;
		m_BMI.bmiHeader.biHeight      = -Ht;		// HARD CODED (HACK??) - negative height to create a top-down DIB
		m_BMI.bmiHeader.biPlanes      = 1;			// ???
		m_BMI.bmiHeader.biBitCount    = BitsPerPixel;
		m_BMI.bmiHeader.biCompression = BI_RGB;
		void *pBitmapBits;
		m_hBitmap = CreateDIBSection(0, &m_BMI, DIB_RGB_COLORS, &pBitmapBits, 0, 0);
		if (m_hBitmap){
			m_pData = (uint8_t*)pBitmapBits;
			if (m_MemDC.CreateCompatibleDC(pDC)){
				m_MemDC.SelectObject(m_hBitmap);
				return true;
			}
		}
	}
	return false;
}

void CMyBitmap::Delete()
{
	if (m_hBitmap){
		m_MemDC.DeleteDC();
		DeleteObject(m_hBitmap);
		m_hBitmap = nullptr;
	}
}

LRESULT CIPCameraDlg::OnFrameReady(WPARAM wParam, LPARAM lParam)
{
	// WM_APP_FRAME_READY message handler.
	PROCESSED_FRAME *pBuf = m_ProcessorToGui.AcquireRead();
	if (pBuf){
		if (m_pDC){
			/*	VERSION REQUIRING EXTRA COPY, as it uses an intermediate 'new' buffer rather than directly using a CreateDIBSection created buffer.
			if (InitDisplayDC(m_pDC, &m_Pic, pBuf->Wd, pBuf->Ht)){
				memcpy(m_Pic.pData, pBuf->pData, pBuf->Size);
				m_pDC->BitBlt(0, 0, pBuf->Wd, pBuf->Ht, &m_Pic.DC, 0, 0, SRCCOPY);
				// alternatively, call InvalidateRect(...) and do the painting in OnPaint().
			}*/

			CMyBitmap *pMDC = (CMyBitmap*)pBuf->pGuiData;
			if (pMDC)
				m_pDC->BitBlt(0, 0, pBuf->Wd, pBuf->Ht, &pMDC->m_MemDC, 0, 0, SRCCOPY);
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
		HWND hWnd = (HWND)pParam;
		// Post message - non-blocking will put in receiving window (GUI thread) message queue.
		PostMessage(hWnd, WM_APP_FRAME_READY, 0, 0);
	}
}

LRESULT CIPCameraDlg::OnCameraReady(WPARAM wParam, LPARAM lParam)
{
	// WM_APP_CAMERA_READY message handler.
	int Wd = (int)wParam;
	int Ht = (int)lParam;
	int LineSize = Wd * 3;
	// LineSize = image format (RGB 24, greyscale, ect) must be specified when opening the camera, that is how we will know it here.
	// @@@@@@@@@ HARD CODED FOR NOW.

	bool Ok = true;
	for (int i=0; i<num_entries(m_ProcessedFrame) && Ok; i++){

		PROCESSED_FRAME &PF = m_ProcessedFrame[i];
		PF.Wd      = Wd;
		PF.Ht      = Ht;
		PF.Span    = LineSize;
		PF.Planes  = PF.Span / PF.Wd;
		PF.Padding = PF.Span - (PF.Wd * PF.Planes);
	
		if (i < num_entries(m_GuiData)){
			if (m_GuiData[i].Create(m_pDC, Wd, Ht, 24)){
				PF.pGuiData = (void*)&m_GuiData[i];
				PF.pData    = m_GuiData[i].m_pData;
			}
		}
		else {
			// Rather than declaring m_GuiData (with index count matching m_ProcessedFrame), I could instead allocate memory for 
			// PF.pGuiData in this loop. The disadvantge of doing that is the added complication to the cleanup code freeing the memory.
			::AfxMessageBox(_T("error - num_entries(m_ProcessedFrame) != num_entries(m_GuiData) "));
			Ok = false;
		}
	}
	if (Ok){
		m_ProcessorToGui.AssignBuffer(m_ProcessedFrame, num_entries(m_ProcessedFrame));
		m_ImgProcThread.Start(&m_CameraToProcessor, &m_ProcessorToGui, FrameReadyCallback, (void*)m_hWnd);
	}
	return 0;
}

void CameraReadyCallback(int Wd, int Ht, void *pParam)
{
	// This is called in the camera thread, not in the GUI thread. 
	if (pParam){
		TRACE("\n  CameraReadyCallback %d x %d  ", Wd, Ht);
		HWND hWnd = (HWND)pParam;

		// Send message - blocks execution until the receiving window (GUI thread) processes the message.
		// SendMessage(WM_APP_CAMERA_READY, (WPARAM)Wd, (LPARAM)Ht);

		// Using SendMessage is problematic here because, here is the scenario :
		// Camera thread starts. Calls this callback but at that instant the window in closed, and m_CamThread.Terminate() is called in OnDestroy.
		// Next, m_CamThread.Terminate() blocks the GUI thread until the camera thread terminates.
		// But the camera thread cant terminate because SendMessage won't return until WM_APP_CAMERA_READY msg is processed.
		// Conclusion - dont use SendMessage.
		PostMessage(hWnd, WM_APP_CAMERA_READY, (WPARAM)Wd, (LPARAM)Ht);
	}
}

void CIPCameraDlg::OnBnClickedBtnConnect()
{
	static int once=0;
	if (once++ == 0){
		m_CameraToProcessor.AssignBuffer(m_Frame, num_entries(m_Frame));
		// to do - specify required image format (RGB 24, greyscale, ect).
		m_CamThread.Start(Utf16ToUtf8(m_CameraURL.GetString()), IMAGEFORMAT::IMGFMT_RGB24, &m_CameraToProcessor, CameraReadyCallback, (void*)m_hWnd);
	}
}


// image processing ideas...
//  - plate finder
//  - OpenCV for OCR, motion detection, and what else does it do?
//  - Darknet for object detection
//  - maybe Ollama and a LLM ( Gemma4 ).

