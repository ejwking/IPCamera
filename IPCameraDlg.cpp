// IPCameraDlg.cpp : implementation file
//

#include "pch.h"
#include "framework.h"
#include "IPCameraDlg.h"
#include "afxdialogex.h"


#ifdef _DEBUG
#define new DEBUG_NEW
#endif


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
	m_Setup.ReadConfigFile();
	return TRUE;  // return TRUE  unless you set the focus to a control
}

void CIPCameraDlg::OnDestroy()
{
	CDialogEx::OnDestroy();
	// TODO: Add your message handler code here
	m_CamInterface.Shutdown();
}

// If you add a minimize button to your dialog, you will need the code below to draw the icon.
// For MFC applications using the document/view model, this is automatically done for you by the framework.
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

LRESULT CIPCameraDlg::OnFrameReady(WPARAM wParam, LPARAM lParam)
{
	// will need WM_APP_FRAME_READY per camera, eg, WM_APP_FRAME_READY_CAM1, WM_APP_FRAME_READY_CAM2, ..

	// WM_APP_FRAME_READY message handler.
	m_CamInterface.FrameReadyMessageHandler(m_pDC, 1.0f, 20, 20);
	return 0;
}

LRESULT CIPCameraDlg::OnCameraReady(WPARAM wParam, LPARAM lParam)
{
	// WM_APP_CAMERA_READY message handler.
	m_CamInterface.CameraReadyMessageHandler((int)wParam, (int)lParam, m_pDC);
	return 0;
}

void CIPCameraDlg::OnBnClickedBtnConnect()
{
	//for (int i=0; i<m_Setup.m_NumCams; i++){

	if (m_Setup.m_Camera[0].Url != "")
		m_CamInterface.Start(m_Setup.m_Camera[0].Url, m_hWnd, WM_APP_CAMERA_READY, WM_APP_FRAME_READY);
}

