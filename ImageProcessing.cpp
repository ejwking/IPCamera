

#include "pch.h"
#include "ImageProcessing.h"

bool Image_Processing(CImageMem *pImg)
{
	// Image processing logic here, eg, object detection.
	// write results to a log file or database.

	for(int i=0; i<pImg->Ht; i++){
		uint8_t *pLine = pImg->pBits + (i * pImg->Span);
		for(int j=0; j<pImg->Wd; j++){
			uint8_t *r = &pLine[j*pImg->Planes + 0];
			uint8_t *g = &pLine[j*pImg->Planes + 1];
			uint8_t *b = &pLine[j*pImg->Planes + 2];

			// do something with r,g,b
			*r = 255 - *r;	// invert red channel
			//	*g = 255 - *g;	// invert green channel
			//	*b = 255 - *b;	// invert blue channel
		}
	}
	return true;
}