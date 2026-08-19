
// IPCameraDlg.h : header file
//

#pragma once

#include "Camera.h"


#define NUM_IMGPROC_FRAMES	3


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


class CIPCamera
{
	// put everything in here for controlling it, because 
	// it shouldnt be mixed up in CIPCameraDlg ???
};


// CIPCameraDlg dialog
class CIPCameraDlg : public CDialogEx
{
// Construction
public:
	CIPCameraDlg(CWnd* pParent = nullptr);	// standard constructor

	CFrame m_Frame[3];
	SPSCRingBuffer<CFrame> m_CameraToProcessor;
	CCameraThread m_CamThread;

	CMyBitmap m_GuiData[NUM_IMGPROC_FRAMES];
	PROCESSED_FRAME m_ProcessedFrame[NUM_IMGPROC_FRAMES];
	SPSCRingBuffer<PROCESSED_FRAME> m_ProcessorToGui;
	CImageProcessingThread m_ImgProcThread;

	CString m_CameraURL;
	CDC    *m_pDC=nullptr;

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
	afx_msg LRESULT OnCameraReady(WPARAM wParam, LPARAM lParam);
	afx_msg LRESULT OnFrameReady(WPARAM wParam, LPARAM lParam);
	afx_msg void OnBnClickedBtnConnect();
};
