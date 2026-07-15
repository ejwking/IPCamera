
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



#define IP_CAMERA_URL "rtsp://username:password@192.168.0.31:554/stream1"


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

//	if (m_Camera.Open("C:\\Users\\edwar\\Desktop\\lesser used media\\tiktok vids\\Download.mp4"))
	if (m_Camera.Open(IP_CAMERA_URL))
	{
		TRACE("\n\nconnected\n");
	}
	else{
		m_Camera.Close(); // Improve Open() so that it cleans up after itself if it fails. For now, just call Close() to clean up.
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
		m_pDC = this->GetDC();
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
	Frame rgbFrame;	// do not declare in loop as it will allocate memory every time in the constructor. Declare it here and reuse.

	// To do:
	// The Grab() function is a blocking call. It will not return until a frame is available, or the stream ends. 
	// If you want to run this in a separate thread, you can use std::thread to run the Grab() loop in a background thread. (dont use MFC's AfxBeginThread()) 

	while (m_Camera.Grab() 
		&& frameNumber<1
		)
	{
		frameNumber++;

		const Frame& frame = m_Camera.CurrentFrame();

		if (converter.Convert(frame, rgbFrame))
		{
			// rgbFrame now contains RGB24 pixels

			TRACE("\nFrame %d   - w %d, h %d, %d, %s  ", frameNumber, rgbFrame.Width(), rgbFrame.Height(), frame.PixelFormat(), frame.PixelFormatName());

			for(int y = 0; y < rgbFrame.Height(); y++)
			{
				const uint8_t* scanline = rgbFrame.ScanLine(y);
				for(int x = 0; x < rgbFrame.Width(); x++)
				{
					uint8_t r = scanline[x * 3 + 0];
					uint8_t g = scanline[x * 3 + 1];
					uint8_t b = scanline[x * 3 + 2];
					// Set the pixel on the dialog's device context
					m_pDC->SetPixel(x,y,RGB(r,g,b));
				}
			}
		}
	}
}

// KEEP IN MIND PORTABILITY for linux:
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//
// This code is written for Windows and MFC. If I want to port it to Linux, I will need to replace the MFC parts (CDialogEx, CWnd, etc.) 
// with a cross-platform GUI library like Qt or wxWidgets. The Camera class itself is platform-independent, but the dialog code is not.
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~




