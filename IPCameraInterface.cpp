
#include "pch.h"
#include "IPCameraInterface.h"
#include "utilities.h"


bool CMyBitmap::Create(CDC *pDC, int Wd, int Ht, int BitsPerPixel)
{
	if (m_hBitmap){
		::AfxMessageBox(_T("error - CMyBitmap. Bitmap already created "));
	}
	else {
		memset(&m_BMI.bmiHeader, 0, sizeof(m_BMI.bmiHeader));
		m_BMI.bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
		m_BMI.bmiHeader.biWidth       = Wd;
		m_BMI.bmiHeader.biHeight      = -Ht;		// HARD CODED (HACK??) - negative height to create a top-down DIB
		m_BMI.bmiHeader.biPlanes      = 1;			// ???
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

bool CIPCameraInterface::Start(std::string CameraURL, HWND hWnd, UINT CameraReadyMsg, UINT FrameReadyMsg)
{
	if (m_Init == 0){
		m_Init = 1;
		m_hWnd = hWnd;
		m_CameraReadyMsg = CameraReadyMsg;
		m_FrameReadyMsg = FrameReadyMsg;
		m_CameraToProcessor.AssignBuffer(m_Frame, num_entries(m_Frame));
		// to do - specify required image format (RGB 24, greyscale, ect).
		m_CamThread.Start(CameraURL, IMAGEFORMAT::IMGFMT_RGB24, &m_CameraToProcessor, Static_CameraReadyCallback, (void*)this);
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
	// This is called in the camera thread, not in the GUI thread. 
	if (pParam){
		CIPCameraInterface *ipc = (CIPCameraInterface*)pParam;
		// SendMessage(...) - blocks execution until the receiving window (GUI thread) processes the message.
		// Using SendMessage is problematic here because, here is the scenario :
		// Camera thread starts. Calls this callback but at that instant the window in closed, and m_CamThread.Terminate() is called in OnDestroy.
		// Next, m_CamThread.Terminate() blocks the GUI thread until the camera thread terminates.
		// But the camera thread cant terminate because SendMessage won't return until WM_APP_CAMERA_READY msg is processed.
		// Conclusion - dont use SendMessage.
		PostMessage(ipc->m_hWnd, ipc->m_CameraReadyMsg, (WPARAM)Wd, (LPARAM)Ht);
	}
}

void CIPCameraInterface::Static_FrameReadyCallback(int Code, void *pParam)
{
	// This is called in the image processing thread, not in the GUI thread. 
	if (pParam){
		CIPCameraInterface *ipc = (CIPCameraInterface*)pParam;
		// Post message - non-blocking will put in receiving window (GUI thread) message queue.
		PostMessage(ipc->m_hWnd, ipc->m_FrameReadyMsg, 0, 0);
	}
}

bool CIPCameraInterface::CameraReadyMessageHandler(int Wd, int Ht, CDC *pScreen)
{
	int LineSize = Wd * 3;
	// LineSize = image format (RGB 24, greyscale, ect) must be specified when opening the camera, that is how we will know it here.
	// @@@@@@@@@ HARD CODED FOR NOW.

	for (int i=0; i<num_entries(m_ProcessedFrame); i++){
		PROCESSED_FRAME &PF = m_ProcessedFrame[i];
		PF.Wd      = Wd;
		PF.Ht      = Ht;
		PF.Span    = LineSize;
		PF.Planes  = PF.Span / PF.Wd;
		PF.Padding = PF.Span - (PF.Wd * PF.Planes);

		if (i < num_entries(m_GuiData)){
			if (m_GuiData[i].Create(pScreen, Wd, Ht, 24)){
				PF.pGuiData = (void*)&m_GuiData[i];
				PF.pData    = m_GuiData[i].m_pData;
			}
		}
		else {
			// Rather than declaring m_GuiData (with index count matching m_ProcessedFrame), I could instead allocate memory for 
			// PF.pGuiData in this loop. The disadvantge of doing that is the added complication to the cleanup code freeing the memory.
			::AfxMessageBox(_T("error - num_entries(m_ProcessedFrame) != num_entries(m_GuiData) "));
			return  false;
		}
	}
	m_ProcessorToGui.AssignBuffer(m_ProcessedFrame, num_entries(m_ProcessedFrame));
	m_ImgProcThread.Start(&m_CameraToProcessor, &m_ProcessorToGui, Static_FrameReadyCallback, (void*)this);
	return true;
}

void CIPCameraInterface::FrameReadyMessageHandler(CDC *pScreen)
{
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
			if (pMB)
				pScreen->BitBlt(0, 0, pBuf->Wd, pBuf->Ht, &pMB->m_MemDC, 0, 0, SRCCOPY);
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
