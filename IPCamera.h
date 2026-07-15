
// IPCamera.h : main header file for the PROJECT_NAME application
//

#pragma once

#ifndef __AFXWIN_H__
	#error "include 'pch.h' before including this file for PCH"
#endif

#include "resource.h"		// main symbols


// CIPCameraApp:
// See IPCamera.cpp for the implementation of this class
//

class CIPCameraApp : public CWinApp
{
public:
	CIPCameraApp();

// Overrides
public:
	virtual BOOL InitInstance();

// Implementation

	DECLARE_MESSAGE_MAP()
};

extern CIPCameraApp theApp;
