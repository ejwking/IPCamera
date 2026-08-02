
// IPCameraDlg.h : header file
//

#pragma once

#include "Camera.h"


struct MEMORYDC
{
	HBITMAP    hBitmap=nullptr;
	BITMAPINFO bmi={0};
	uint8_t	  *pBits=nullptr;
	CDC        DC;	// or HDC? ..CDC better cos I can use GDI functions using class '->' syntax.
//	int        Wd=0, Ht=0;
	int        InitBitmap=0, InitDC=0;
};


// CIPCameraDlg dialog
class CIPCameraDlg : public CDialogEx
{
// Construction
public:
	CIPCameraDlg(CWnd* pParent = nullptr);	// standard constructor

	CCameraThread m_CamThread;

	CString   m_CameraURL;
	MEMORYDC  m_Pic;
	CDC      *m_pDC=nullptr;

	void FreeBitmapObjects(MEMORYDC * pMDC);
	int  InitDisplayDC(CDC *pDC, MEMORYDC *pMemDC, int Wd, int Ht);
	void RgbFrameDrawTest(const CFrame& rgbFrame);

// Dialog Data
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_IPCAMERA_DIALOG };
#endif

protected:
	virtual void DoDataExchange(CDataExchange* pDX);	// DDX/DDV support

// Implementation
protected:
	HICON m_hIcon;

	// Generated message map functions
	virtual BOOL OnInitDialog();
	afx_msg void OnPaint();
	afx_msg HCURSOR OnQueryDragIcon();
	DECLARE_MESSAGE_MAP()

public:
	afx_msg void OnDestroy();
	afx_msg LRESULT OnFrameReady(WPARAM wParam, LPARAM lParam);
	afx_msg void OnBnClickedBtnConnect();
};
