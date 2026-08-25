
#include "pch.h"
#include "IPCameraInterface.h"
#include "utilities.h"


bool CMyBitmap::Create(CDC *pDC, int Wd, int Ht, int BitsPerPixel, bool TopDown)
{
	if (m_hBitmap){
		::AfxMessageBox(_T("error - CMyBitmap. Bitmap already created "));
	}
	else {
		memset(&m_BMI.bmiHeader, 0, sizeof(m_BMI.bmiHeader));
		m_BMI.bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
		m_BMI.bmiHeader.biWidth       = Wd;
		m_BMI.bmiHeader.biHeight      = TopDown ? -Ht : Ht;	// negative height to create a top-down DIB.
		m_BMI.bmiHeader.biPlanes      = 1;					// ???
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

// ############################################################################################################################################

bool CIPCameraManager::InitialiseSetup(const std::string& ConfigPath, HWND hWnd, uint32_t CameraReadyMsg, uint32_t FrameReadyMsg)
{
	CConfigFile cfg;
	if (!cfg.load(ConfigPath)){
		if (!cfg.getErrors().empty())
			for (auto& err : cfg.getErrors())
				TRACE(_T("\n config.txt error - %s "), Utf8(err.c_str()));
		return false;
	}

	// ~~~~~~~~~~~~~~~~~~~~~
	// field : camera_url_XX
	// 
	// in config file the ip camera list will be indexed like this:
	// camera_url_1 = 192.168.xxxx
	// camera_url_2 = 192.168.xxxx
	// camera_url_3 = 192.168.xxxx
	// But we dont care what the indexes in this list are, just as long as each one is different, and between 0 and 99.
	m_Setup.clear();
	for (int i=0; i<100; i++){
		std::string name = "camera_url_" + std::to_string(i);
		std::string url = cfg.getString(name, "");
		if (!url.empty()){
			int index = (int)m_Setup.size();
		//	if (index < MaxCameras){
				IPCAMERASETUP S;
				S.Url = url;
				S.CameraReadyMsg = CameraReadyMsg;
				S.FrameReadyMsg  = FrameReadyMsg;
				S.MessageSubCode = index;
				S.hWnd           = hWnd;
				m_Setup.push_back(S);
		//	}
		}
	}

	// ~~~~~~~~~~~~~~~~~~~~~
	// field : display_enabled
	m_DisplayEnabled = cfg.getBool("display_enabled");

	return (m_Setup.size() > 0);
}

bool CIPCameraManager::StartStreams()
{
	if (!m_pCams){
		int Size = (int)m_Setup.size();
		if (Size > 0){
			m_pCams = new CIPCameraInterface[Size];
			if (m_pCams){
				for (int i=0; i<Size; i++)
					m_pCams[i].Start(m_Setup[i]);
				return true;
			}
			::AfxMessageBox(_T("StartAllCameras - memory allocation failed"));
		}
	}
	return false;
}

void CIPCameraManager::TerminateStreams()
{
	if (m_pCams){

		int Num = (int)m_Setup.size();

		// why cant i do num_entries(m_pCams) here?
		// because it would be safer, better form

		for (int i=0; i<Num; i++)
			m_pCams[i].Shutdown();

		// For new (single object), use delete
		// For new[] (array), use delete[]
		delete[] m_pCams;
		m_pCams = nullptr;
	}
}

// ############################################################################################################################################

bool CIPCameraInterface::Start(const IPCAMERASETUP& Setup)
{
	if (m_Init == 0){
		m_Init = 1;
		m_Setup = Setup; // make copy of.
		m_CameraToProcessor.AssignBuffer(m_Frame, num_entries(m_Frame));
		// to do - specify required image format (RGB 24, greyscale, ect).
		m_CamThread.Start(m_Setup.Url, IMAGEFORMAT::IMGFMT_RGB24, &m_CameraToProcessor, Static_CameraReadyCallback, (void*)this);
	}
	return false;
}

void CIPCameraInterface::Shutdown()
{
	m_CamThread.Terminate();
	m_ImgProcThread.Terminate();
}

void CIPCameraInterface::Static_CameraReadyCallback(int Wd, int Ht, void *pParam)
{
	// Called in the camera thread and will post a message to the GUI thread. 
	if (pParam){
		CIPCameraInterface *ipc = (CIPCameraInterface*)pParam;
		// SendMessage(...) - blocks execution until the receiving window (GUI thread) processes the message.
		// Using SendMessage is problematic here because, here is the scenario :
		// Camera thread starts. Calls this callback but at that instant the window in closed, and m_CamThread.Terminate() is called in OnDestroy.
		// Next, m_CamThread.Terminate() blocks the GUI thread until the camera thread terminates.
		// But the camera thread cant terminate because SendMessage won't return until WM_APP_CAMERA_READY msg is processed.
		// Conclusion - dont use SendMessage.
		LPARAM lp = MAKE_LPARAM2(Wd, Ht);
		PostMessage(ipc->m_Setup.hWnd, (UINT)ipc->m_Setup.CameraReadyMsg, (WPARAM)ipc->m_Setup.MessageSubCode, lp);
	}
}

// When i hit a breakpoint in GUI thread (or was it img proc thread?), windows drops the frame ready msgs. Apparaently windows msg queue has 10,000 slots.
// So if camera was doing 30fps, 10,000/30 = 333 seconds (5.5 minutes) until queue is full. Strange because I'm sure i didnt stop at breakpoint that
// long, try it again.
// Anyway if frame ready messages do get dropped, then the ring buffer becames full and then cant post any new frame ready messages and then can never be 
// emptied, hence no more frames get processed or come through to the GUI.
// Although this is a debug/breakpoint scenario, could this scenario occur in normal running if the computer became bogged down?
// ..maybe put in some safety code, just incase.

void CIPCameraInterface::Static_FrameReadyCallback(int Code, void *pParam)
{
	// Called in the image processing thread and will post a message to the GUI thread. 
	if (pParam){
		CIPCameraInterface *ipc = (CIPCameraInterface*)pParam;
		// Post message - non-blocking will put in receiving window (GUI thread) message queue.
		PostMessage(ipc->m_Setup.hWnd, (UINT)ipc->m_Setup.FrameReadyMsg, (WPARAM)ipc->m_Setup.MessageSubCode, 0);
	}
}

bool CIPCameraInterface::CameraReadyMessageHandler(int Wd, int Ht, CDC *pScreen)
{
	// GUI thread message handler.
	for (int i=0; i<num_entries(m_ProcessedFrame); i++){
		PROCESSED_FRAME &PF = m_ProcessedFrame[i];

		// HARD CODED
		// Desired image format (eg, RGB 24, greyscale, ect) should be specified at startup, and passed to img proc thread.
		// Currently img proc thread just outputs at RGB24, hence hard coded below - LineSize = Wd * 3;
		PF.Wd       = Wd;
		PF.Ht       = Ht;
		PF.LineSize = Wd * 3;
		PF.Planes   = PF.LineSize / PF.Wd;
		PF.Padding  = PF.LineSize - (PF.Wd * PF.Planes);

		if (i < num_entries(m_GuiData)){
			if (m_GuiData[i].Create(pScreen, Wd, Ht, 24, true)){
				PF.pGuiData = (void*)&m_GuiData[i];
				PF.pData    = m_GuiData[i].m_pData;
			}
		}
		else {
			// m_GuiData and m_ProcessedFrame are defined separately, but have the same array size as each m_ProcessedFrame element contains a m_GuiData. I could 
			// instead allocate memory for PF.pGuiData in this loop. But I prefer not to allocate as that adds complication to the cleanup code as it will need to free the memory.
			::AfxMessageBox(_T("error - num_entries(m_ProcessedFrame) != num_entries(m_GuiData) "));
			return  false;
		}
	}
	// Now the camera has started we can start the image processing thread.
	m_ProcessorToGui.AssignBuffer(m_ProcessedFrame, num_entries(m_ProcessedFrame));
	m_ImgProcThread.Start(&m_CameraToProcessor, &m_ProcessorToGui, Static_FrameReadyCallback, (void*)this);
	return true;
}

void CIPCameraInterface::FrameReadyMessageHandler(CDC *pScreen, float Scale, int X, int Y)
{
	// GUI thread message handler.
	PROCESSED_FRAME *pBuf = m_ProcessorToGui.AcquireRead();
	if (pBuf){
		if (pScreen){
			/*	VERSION REQUIRING EXTRA COPY, as it uses an intermediate 'new' buffer rather than directly using a CreateDIBSection created buffer.
			if (InitDisplayDC(pScreen, &m_Pic, pBuf->Wd, pBuf->Ht)){
				memcpy(m_Pic.pData, pBuf->pData, pBuf->Size);
				pScreen->BitBlt(0, 0, pBuf->Wd, pBuf->Ht, &m_Pic.DC, 0, 0, SRCCOPY);
				// alternatively, call InvalidateRect(...) and do the painting in OnPaint().
			}*/
			CMyBitmap *pMB = (CMyBitmap*)pBuf->pGuiData;
			if (pMB){
				if (pMB->IsCreated()){
					DrawImgProcOutput(&pMB->m_MemDC, &pBuf->ImgProcOut);
					if (Scale == 1.0f)
						pScreen->BitBlt(X, Y, pBuf->Wd, pBuf->Ht, &pMB->m_MemDC, 0, 0, SRCCOPY);
					else {
						int Wd = (int)((float)pBuf->Wd * Scale);
						int Ht = (int)((float)pBuf->Ht * Scale);
						pScreen->SetStretchBltMode(HALFTONE);
						pScreen->StretchBlt(X, Y, Wd, Ht, &pMB->m_MemDC, 0, 0, pBuf->Wd, pBuf->Ht, SRCCOPY);
					}
				}
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
}

void CIPCameraInterface::DrawImgProcOutput(CDC *pCDC, IMG_PROC_OUTPUT *pIPO)
{
	int Style = PS_SOLID;// PS_DOT;
	if(Style != PS_SOLID){
		pCDC->SetBkMode(OPAQUE);
		pCDC->SetBkColor(RGB(0,0,0));
	}
	// Temporary hard coding for testing.
	// Shouldnt be recreating pen every time, can create once and re-use.
	// ..Have a separate module/class for drawing.
	CPen *oldpen, pen(Style, 3, RGB(155,0,155));
	oldpen = pCDC->SelectObject(&pen);
	pCDC->SetROP2(R2_COPYPEN);
	int Max = num_entries(pIPO->Pt);
	for (int i=0; i<Max-1; i++){
		pCDC->MoveTo(pIPO->Pt[i].x, pIPO->Pt[i].y);
		pCDC->LineTo(pIPO->Pt[i+1].x, pIPO->Pt[i+1].y);
	}
	pCDC->MoveTo(pIPO->Pt[0].x, pIPO->Pt[0].y);
	pCDC->LineTo(pIPO->Pt[Max-1].x, pIPO->Pt[Max-1].y);
	//pCDC->PolyDraw(...);
	pCDC->SelectObject(oldpen);
}
