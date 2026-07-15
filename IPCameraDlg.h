
// IPCameraDlg.h : header file
//

#pragma once

#include "Camera.h"


// CIPCameraDlg dialog
class CIPCameraDlg : public CDialogEx
{
// Construction
public:
	CIPCameraDlg(CWnd* pParent = nullptr);	// standard constructor

	Camera m_Camera;

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
	afx_msg void OnBnClickedBtnTest();
};
