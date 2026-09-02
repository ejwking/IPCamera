
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
	if (!m_CamSetup.empty()){
		// to do - add report error
		return false;
	}

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
	m_CamSetup.clear();
	for (int i=0; i<100; i++){
		std::string name = "camera_url_" + std::to_string(i);
		std::string url = cfg.getString(name, "");
		if (!url.empty()){
			int index = (int)m_CamSetup.size();
			if (index >= SAFE_MAX_CAMERAS){
				// to do - add report error
			}
			else {
				IPCAMERASETUP S;
				S.Url            = url;
				S.CameraReadyMsg = CameraReadyMsg;
				S.FrameReadyMsg  = FrameReadyMsg;
				S.MessageSubCode = index;
				S.hWnd           = hWnd;
				m_CamSetup.push_back(S);
			}
		}
	}
	// ~~~~~~~~~~~~~~~~~~~~~
	// field : display_enabled
	m_DisplayEnabled = cfg.getBool("display_enabled");

	InitGridLayout();
	return (m_CamSetup.size() > 0);
}

bool CIPCameraManager::StartStreams()
{
	if (m_pCams){
		// error already started, must be terminated first
	}
	else {
		int Count = (int)m_CamSetup.size();
		if (Count > 0){
			m_pCams = new CIPCameraInterface[Count];
			if (m_pCams){
				for (int i=0; i<Count; i++)
					m_pCams[i].Start(m_CamSetup[i]);
				return true;
			}
			::AfxMessageBox(_T("StartAllCameras - memory allocation failed"));
		}
	}
	return false;
}

void CIPCameraManager::TerminateStreams()
{
	int Count = (int)m_CamSetup.size();
	m_CamSetup.clear();
	if (m_pCams){
		for (int i=0; i<Count; i++)
			m_pCams[i].Shutdown();
		// For new (single object), use delete
		// For new[] (array), use delete[]
		delete[] m_pCams;
		m_pCams = nullptr;
	}
}

bool CIPCameraManager::CameraReadyMessageHandler(WPARAM wParam, LPARAM lParam, CDC *pScreen)
{
	// GUI thread message handler.
	int CameraIndex = (int)wParam;
	return m_pCams[CameraIndex].CameraReadyMessageHandler(pScreen);
}

void CIPCameraManager::FrameReadyMessageHandler(WPARAM wParam, LPARAM lParam, CDC *pScreen, int WindowCX, int WindowCY)
{
	// GUI thread message handler.
	int NumCams = m_CamSetup.size();
	int CamIdx  = (int)wParam;
	int Code    = (int)lParam; // unused.
	auto r = m_Grid.GetCellRect(WindowCX, WindowCY, CamIdx);
	m_pCams[CamIdx].FrameReadyMessageHandler(pScreen, r.left, r.top, r.right-r.left, r.bottom-r.top);
}

int CIPCameraManager::DrawGridLayout(CDC *pScreen, int WindowCX, int WindowCY)
{
	int NumCams = m_CamSetup.size();
	if (NumCams > 0){
		for (int i=0; i<NumCams; i++){
			auto r = m_Grid.GetCellRect(WindowCX, WindowCY, i);
			CGdiObject *pOld = pScreen->SelectStockObject(DKGRAY_BRUSH);
			pScreen->Rectangle(r.left, r.top, r.right, r.bottom);
			pScreen->SelectObject(pOld);
		}
		return 1;
	}
	return 0;
}

void CIPCameraManager::InitGridLayout()
{
	int Count = m_CamSetup.size();
	if (Count > 0){
		int Bdr = 5;
		if (Count == 1)
			m_Grid.Initialse(1, 1, Bdr, Bdr);
		else if (Count <= 3)
			// one row, up to 3 columns.
			m_Grid.Initialse(1, Count, Bdr, Bdr);
		else {
			// 2 or more rows. Window divided up with sqrt, ie, 9 cells = 3 row, 3 cols.
			// This is rough and simple way of doing it, I need to consider window aspect ratio and video aspect ratio.
			int rows = static_cast<int>(std::sqrt(Count));
			int cols = (Count + rows - 1) / rows;   // ceiling division
			m_Grid.Initialse(rows, cols, Bdr, Bdr);
		}	
	}
}

CIPCameraManager::CIPCameraManager()
{
}

CIPCameraManager::~CIPCameraManager()
{
	TerminateStreams();
}

// ############################################################################################################################################
// ############################################################################################################################################

bool CIPCameraInterface::Start(const IPCAMERASETUP& Setup)
{
	// called in the GUI thread.
	if (m_Init == 0){
		m_Init = 1;
		m_Setup = Setup; // make copy of.
		m_CameraToProcessor.AssignBuffer(m_Frame, num_entries(m_Frame));
		// to do - specify required image format (RGB 24, greyscale, ect).
		m_CamThread.Start(m_Setup.Url, IMAGEFORMAT::IMGFMT_BGR24, &m_CameraToProcessor, Static_CameraReadyCallback, (void*)this);
	}
	return false;
}

void CIPCameraInterface::Shutdown()
{
	m_CamThread.Terminate();
	m_ImgProcThread.Terminate();
	m_Init = 0;
}

void CIPCameraInterface::Static_CameraReadyCallback(CAMERA_READY_CALLBACK_PARAMS)
{
	// Called in the camera thread and will post a message to the GUI thread. 
	if (pParam){
		CIPCameraInterface *ipc = (CIPCameraInterface*)pParam;
		// SendMessage(...) - blocks execution until the receiving window (GUI thread) processes the message.
		// Using SendMessage is problematic here because, here is the scenario :
		// Camera thread starts. Calls this callback but at that instant the window in closed, and m_CamThread.Terminate() is called in OnDestroy.
		// Next, m_CamThread.Terminate() blocks the GUI thread until the camera thread terminates. But the camera thread cant terminate because 
		// SendMessage won't return until WM_APP_CAMERA_READY msg is processed. Conclusion - dont use SendMessage.
		ipc->m_VideoInfo = VI;
		PostMessage(ipc->m_Setup.hWnd, (UINT)ipc->m_Setup.CameraReadyMsg, (WPARAM)ipc->m_Setup.MessageSubCode, 0);
	}
}

// TODO @@@@@@@@@
// When i hit a breakpoint in GUI thread (or was it img proc thread?), windows drops the frame ready msgs. Apparaently windows msg queue has 10,000 slots.
// So if camera was doing 30fps, 10,000/30 = 333 seconds (5.5 minutes) until queue is full. Strange because I'm sure i didnt stop at breakpoint that
// long, try it again.
// Anyway if frame ready messages do get dropped, then the ring buffer becomes full and then cant post any new frame ready messages and then can never be 
// emptied, hence no more frames get processed or come through to the GUI.
// Although this is a debug/breakpoint scenario, could this scenario occur in normal running if the computer became bogged down? Discuss with chatGPT.
// ..to do - put in some safety code for this. Maybe thread should keep a timer, (also needed for fps), and if no frames have been emptied for X period...

void CIPCameraInterface::Static_FrameReadyCallback(FRAME_READY_CALLBACK_PARAMS)
{
	// Called in the image processing thread (CImageProcessingThread) and will post a message to the GUI thread. 
	if (pParam){
		CIPCameraInterface *ipc = (CIPCameraInterface*)pParam;
		// Post message - non-blocking will put in receiving window (GUI thread) message queue.
		PostMessage(ipc->m_Setup.hWnd, (UINT)ipc->m_Setup.FrameReadyMsg, (WPARAM)ipc->m_Setup.MessageSubCode, (LPARAM)Code);
	}
}

bool CIPCameraInterface::CameraReadyMessageHandler(CDC *pScreen)
{
	// GUI thread message handler.
	for (int i=0; i<num_entries(m_ProcessedFrame); i++){
		PROCESSED_FRAME &PF = m_ProcessedFrame[i];

		// HARD CODED
		// Desired image format (eg, RGB 24, greyscale, ect) should be specified at startup, and passed to img proc thread.
		// Currently img proc thread just outputs at RGB24, hence hard coded below - LineSize = Wd * 3;
		PF.Image.Wd       = m_VideoInfo.width;
		PF.Image.Ht       = m_VideoInfo.height;
		PF.Image.LineSize = PF.Image.Wd * 3;
		PF.Image.Planes   = PF.Image.LineSize / PF.Image.Wd;
		PF.Image.Padding  = PF.Image.LineSize - (PF.Image.Wd * PF.Image.Planes);

		if (i < num_entries(m_GuiData)){
			if (m_GuiData[i].Create(pScreen, PF.Image.Wd, PF.Image.Ht, 24, true)){
				PF.pGuiData    = (void*)&m_GuiData[i];
				PF.Image.pData = m_GuiData[i].m_pData;
			}
		}
		else {
			// m_GuiData and m_ProcessedFrame are defined separately, but have the same array size as each m_ProcessedFrame element contains a m_GuiData. I could 
			// instead allocate memory for PF.Image.pGuiData in this loop. But I prefer not to allocate as that adds complication to the cleanup code as it will need to free the memory.
			::AfxMessageBox(_T("error - num_entries(m_ProcessedFrame) != num_entries(m_GuiData) "));
			return  false;
		}
	}
	// Now the camera has started we can start the image processing thread.
	m_ProcessorToGui.AssignBuffer(m_ProcessedFrame, num_entries(m_ProcessedFrame));
	m_ImgProcThread.Start(&m_CameraToProcessor, &m_ProcessorToGui, Static_FrameReadyCallback, (void*)this, m_Setup.MessageSubCode);
	return true;
}

#define GET_X_SCALE(CellWd, VideoWd) ((VideoWd) > (CellWd)) ? (float)(CellWd)/(float)(VideoWd) : 1.0f;
void CIPCameraInterface::FrameReadyMessageHandler(CDC *pScreen, int Cell_Left, int Cell_Top, int Cell_Wd, int Cell_Ht)
{
	// GUI thread frame ready message handler.
	// Strictly for displaying the processed frame (and any associated data) in the GUI, not image processing etc - that is done in the image processing thread.
	// Also this code here will be disabled/not-executed if the video display is disabled, so any processing here would be futile.
	PROCESSED_FRAME *pBuf = m_ProcessorToGui.AcquireRead();
	if (pBuf){
		if (pScreen){
			// VERSION REQUIRING EXTRA COPY, as it uses an intermediate 'new' buffer rather than directly using a CreateDIBSection created buffer.
			// if (InitDisplayDC(pScreen, &m_Pic, pBuf->Wd, pBuf->Ht)){
			//	 std::memcpy(m_Pic.pData, pBuf->pData, pBuf->Size);
			//	 pScreen->BitBlt(0, 0, pBuf->Wd, pBuf->Ht, &m_Pic.DC, 0, 0, SRCCOPY);
			// }
			CMyBitmap *pMB = (CMyBitmap*)pBuf->pGuiData;
			if (pMB){
				if (pMB->IsCreated()){
					DrawImgProcOutput(&pMB->m_MemDC, &pBuf->ImgProcOut);

					// @@@@@ TODO FIX - 
					// below for scaling and fitting picture in cell we make assumption that width is greater than height.
					float Scale = GET_X_SCALE(Cell_Wd, pBuf->Image.Wd);
					if (Scale == 1.0f)
						pScreen->BitBlt(Cell_Left, Cell_Top, pBuf->Image.Wd, pBuf->Image.Ht, &pMB->m_MemDC, 0, 0, SRCCOPY);
					else {
						int Wd = (int)((float)pBuf->Image.Wd * Scale);
						int Ht = (int)((float)pBuf->Image.Ht * Scale);
						int Center_Y = (Ht < Cell_Ht) ? ((Cell_Ht - Ht) / 2) : 0;
						pScreen->SetStretchBltMode(HALFTONE);
						pScreen->StretchBlt(Cell_Left, Cell_Top+Center_Y, Wd, Ht, &pMB->m_MemDC, 0, 0, pBuf->Image.Wd, pBuf->Image.Ht, SRCCOPY);
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
	CPen *oldpen, pen(Style, 5, RGB(155,0,155));
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


/*
void CDisplay::TextOnDisplay(CDC *pDC, char *pText, COLORREF CR, int x, int y, int Size)
{
	pDC->SetBkMode(OPAQUE);//TRANSPARENT
	COLORREF oldBkCol = pDC->SetBkColor(RGB(0,0,0));
	CFont   *oldobj   = pDC->SelectObject((Size==0) ? &m_GdiObjs.Font_16 : &m_GdiObjs.Font_24);
	COLORREF oldTxCol = pDC->SetTextColor(CR);
	pDC->TextOut(x, y, pText);
	pDC->SetBkColor(oldBkCol);
	pDC->SelectObject(oldobj);
	pDC->SetTextColor(oldTxCol);
}
*/