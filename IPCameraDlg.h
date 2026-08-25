
// IPCameraDlg.h : header file
//

#pragma once

#include "IPCamera.h"
#include "IPCameraInterface.h"


// CIPCameraDlg dialog
class CIPCameraDlg : public CDialogEx
{
// Construction
public:
	CIPCameraDlg(CWnd* pParent = nullptr);	// standard constructor

	CIPCameraManager m_CamManager;
	CDC *m_pDC=nullptr;

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
