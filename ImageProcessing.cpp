

#include "pch.h"
#include "ImageProcessing.h"
#include "ObjectDetection.h"


bool Image_Processing(PROCESSED_FRAME *pImg)
{
	// Image processing logic here, eg, object detection.
	// write results to a log file or database.

	for(int i=0; i<pImg->Ht; i++){
		uint8_t *pLine = pImg->pData + (i * pImg->Span);
		for(int j=0; j<pImg->Wd; j++){
			uint8_t *r = &pLine[j*pImg->Planes + 0];
			uint8_t *g = &pLine[j*pImg->Planes + 1];
			uint8_t *b = &pLine[j*pImg->Planes + 2];

			// do something with r,g,b
//			*r = 255 - *r;	// invert red channel
//			*g = 255 - *g;	// invert green channel
//			*b = 255 - *b;	// invert blue channel

			*r = 0;
			*b = 0;
		}
	}

	static int x=0;
	if ((x%50) == 0){
		// testing
		for (int i=0; i<4; i++){
			pImg->ImgProcOut.Pt[i].x = (rand()*pImg->Wd) / RAND_MAX;
			pImg->ImgProcOut.Pt[i].y = (rand()*pImg->Ht) / RAND_MAX;
		}
	}
	x++;
	return true;
}

/*void RgbFrameDrawTest(const CFrame& rgbFrame)
{
	for (int y = 0; y < rgbFrame.Height(); y++){
		const uint8_t* scanline = rgbFrame.ScanLine(y);
		for (int x = 0; x < rgbFrame.Width(); x++){
			uint8_t r = scanline[x * 3 + 0];
			uint8_t g = scanline[x * 3 + 1];
			uint8_t b = scanline[x * 3 + 2];
			m_pDC->SetPixel(x,y,RGB(r,g,b));
		}
	}
}*/