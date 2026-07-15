
// IPCameraDlg.cpp : implementation file
//

#include "pch.h"
#include "framework.h"
#include "IPCamera.h"
#include "IPCameraDlg.h"
#include "afxdialogex.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif


// CIPCameraDlg dialog



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
	ON_BN_CLICKED(IDC_BTN_TEST,&CIPCameraDlg::OnBnClickedBtnTest)
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


	if (m_Camera.Open("C:\\Users\\edwar\\Desktop\\lesser used media\\tiktok vids\\Download.mp4"))
	{
		TRACE("\n\nconnected\n");
	}
	else{
		TRACE("\n\nOpen failed\n");
	}


	return TRUE;  // return TRUE  unless you set the focus to a control
}

// If you add a minimize button to your dialog, you will need the code below
//  to draw the icon.  For MFC applications using the document/view model,
//  this is automatically done for you by the framework.

void CIPCameraDlg::OnPaint()
{
	if (IsIconic())
	{
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
	else
	{
		CDialogEx::OnPaint();
	}
}

// The system calls this function to obtain the cursor to display while the user drags
//  the minimized window.
HCURSOR CIPCameraDlg::OnQueryDragIcon()
{
	return static_cast<HCURSOR>(m_hIcon);
}


void CIPCameraDlg::OnBnClickedBtnTest()
{
	int frameNumber = 0;

	ImageConverter converter;
	Frame rgbFrame;

	while (m_Camera.Grab())
	{
		frameNumber++;

		const Frame& frame = m_Camera.CurrentFrame();

		if (converter.Convert(frame, rgbFrame))
		{
			// rgbFrame now contains RGB24 pixels

			TRACE("\nFrame %d   - w %d, h %d, %d, %s  ", frameNumber, rgbFrame.Width(), rgbFrame.Height(), frame.PixelFormat(), frame.PixelFormatName());
		}
	}
}
