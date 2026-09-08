


#include "pch.h"
#include "ImageProcessing.h"
//#include "ObjectDetection.h"

#include <cstdint>
#include <algorithm>
#include <cstring>

// so we don't get min and min macros from the windows headers
#undef max
#undef min


void CBitmapShrinker::Initialise(int width, int height, int factor)
{
	if (!m_out){
		//	if (width <= 0 || height <= 0) throw std::invalid_argument("Invalid dimensions");
		//	if (factor <= 0) throw std::invalid_argument("Factor must be > 0");
		m_w = width;
		m_h = height;
		m_factor = factor;
		m_outW = width  / factor;
		m_outH = height / factor;

		//	if (m_outW <= 0 || m_outH <= 0)
		//		throw std::invalid_argument("Shrink factor too large");
		// Allocate output buffer (RGB)
		m_out = new uint8_t[m_outW * m_outH * 3];

		// m_out - error check.
	}
}

CBitmapShrinker::~CBitmapShrinker()
{
	if (m_out)
		delete[] m_out;
}

// Perform the shrink
void CBitmapShrinker::Process(const uint8_t* input)
{
	const int F = m_factor;
	for (int y = 0; y < m_outH; ++y){
		for (int x = 0; x < m_outW; ++x){
			// Accumulators for averaging
			uint32_t rSum = 0, gSum = 0, bSum = 0;
			// Top-left of the block in the input
			int inY0 = y * F;
			int inX0 = x * F;
			for (int yy = 0; yy < F; ++yy){
				const uint8_t* rowPtr = input + ((inY0 + yy) * m_w * 3);
				for (int xx = 0; xx < F; ++xx){
					const uint8_t* px = rowPtr + (inX0 + xx) * 3;
					rSum += px[0];
					gSum += px[1];
					bSum += px[2];
				}
			}
			// Average
			uint32_t count = F * F;
			uint8_t r = static_cast<uint8_t>(rSum / count);
			uint8_t g = static_cast<uint8_t>(gSum / count);
			uint8_t b = static_cast<uint8_t>(bSum / count);
			// Write output pixel
			uint8_t* outPx = m_out + (y * m_outW + x) * 3;
			outPx[0] = r;
			outPx[1] = g;
			outPx[2] = b;
		}
	}
}

void CBitmapShrinker::DrawOutputOnBuffer(MDIMAGE *pBuf)
{
	if (m_outH>0 && m_outW>0 && m_out){
		for (int y = 0; y < m_outH; ++y){
			// copy line.
			uint8_t* dest = pBuf->pData + y * pBuf->LineSize;
			uint8_t* src  = m_out + y * m_outW * 3;
			memcpy(dest, src, sizeof(uint8_t) * m_outW * 3);
		}
	}
}

void CMotionDetector::Initialise(const MDIMAGE& image, int rows, int columns, int BitmapReductionFactor, int verticalSample, int horizontalSample)
{
	m_Rows = rows;
	m_Columns = columns;
	m_VerticalSample = std::max(1, verticalSample);
	m_HorizontalSample = std::max(1, horizontalSample);
	m_Motion.resize(m_Rows * m_Columns, 0);

	m_PreviousWd       = 0;
	m_PreviousHt       = 0;
	m_PreviousLineSize = 0;
	m_HavePreviousFrame = false;

	m_Shrunk.Initialise(image.Wd, image.Ht, BitmapReductionFactor);
}

//#define USE_GREYSCALE_CALCULATION
const uint8_t* CMotionDetector::Process(const MDIMAGE& image)
{
	// Basic validation
	if (image.pData == nullptr ||
		image.Wd <= 0 ||
		image.Ht <= 0 ||
		image.LineSize < image.Wd * 3 ||
		m_Rows <= 0 ||
		m_Columns <= 0)
	{
		std::fill(m_Motion.begin(), m_Motion.end(), 0);
		return m_Motion.data();
	}

	// Check whether the image layout has changed.
	if (image.Wd != m_PreviousWd ||
		image.Ht != m_PreviousHt ||
		image.LineSize != m_PreviousLineSize)
	{
		// Previous frame has exactly the same layout as the source image.
		if (m_PreviousFrame)	// quick hack, not wrong but messy. actually the image size will not, and must not, change.
			delete[] m_PreviousFrame;

		m_PreviousFrame = new uint8_t[image.LineSize * image.Ht];
		m_PreviousWd        = image.Wd;
		m_PreviousHt        = image.Ht;
		m_PreviousLineSize  = image.LineSize;
		m_HavePreviousFrame = false;
	}

	if (!m_HavePreviousFrame){
		// First frame - There is nothing to compare it against.
		std::memcpy(m_PreviousFrame, image.pData, static_cast<size_t>(image.LineSize) * static_cast<size_t>(image.Ht));
		std::fill(m_Motion.begin(), m_Motion.end(), 0);
		m_HavePreviousFrame = true;
		return m_Motion.data();
	}

	// Clear output
	std::fill(m_Motion.begin(), m_Motion.end(), 0);

	// Cell dimensions
	int cellWidth  = image.Wd / m_Columns;
	int cellHeight = image.Ht / m_Rows;

	// Process cells
	for (int row = 0; row < m_Rows; ++row){
		int yStart = row * cellHeight;
		int yEnd = (row == m_Rows - 1) ? image.Ht : yStart + cellHeight;

		for (int col = 0; col < m_Columns; ++col){
			int xStart = col * cellWidth;
			int xEnd = (col == m_Columns - 1) ? image.Wd : xStart + cellWidth;
			int cellWidthActual = xEnd - xStart;
			int cellHeightActual = yEnd - yStart;

			// Because we are sampling the image, the number of pixels actually examined is smaller than the physical cell size.
			int sampledWidth = (cellWidthActual + m_HorizontalSample - 1) / m_HorizontalSample;
			int sampledHeight = (cellHeightActual + m_VerticalSample - 1) / m_VerticalSample;

			int totalSampledPixels = sampledWidth * sampledHeight;
			double requiredPixels = totalSampledPixels * (m_MotionThreshold / 100.0);

			int changedPixels = 0;

			// Sample lines vertically
			for (int y = yStart; y < yEnd; y += m_VerticalSample){
				uint8_t* current = image.pData + static_cast<size_t>(y) * image.LineSize + static_cast<size_t>(xStart) * 3;
				uint8_t* previous = m_PreviousFrame + static_cast<size_t>(y) * m_PreviousLineSize + static_cast<size_t>(xStart) * 3;

				// Sample pixels horizontally
				for (int x = xStart; x < xEnd; x += m_HorizontalSample){

#ifdef USE_GREYSCALE_CALCULATION
					// RGB24 -> simple brightness value.
					int currentGray  = (current[0] + current[1] + current[2]) / 3;
					int previousGray = (previous[0] + previous[1] + previous[2]) / 3;
					int difference   = std::abs(currentGray - previousGray);
#else
					int dr = std::abs(current[0] - previous[0]);
					int dg = std::abs(current[1] - previous[1]);
					int db = std::abs(current[2] - previous[2]);

					// Use the largest RGB difference.
					//int difference = std::max({ dr, dg, db });
					int difference = std::max(dr, dg);
					difference = std::max(difference, db);
#endif
					if (difference >= m_PixelThreshold){
						++changedPixels;
						// Enough changed pixels to classify this cell as containing motion.
						if (changedPixels >= requiredPixels){
							m_Motion[row * m_Columns + col] = 1;
							break;
						}
					}
					// Move to the next sampled pixel.
					current += static_cast<size_t>(m_HorizontalSample) * 3;
					previous += static_cast<size_t>(m_HorizontalSample) * 3;
				}
				// Cell already classified as motion.
				if (m_Motion[row * m_Columns + col])
					break;
			}
		}
	}

	// Save current frame.
	std::memcpy(m_PreviousFrame, image.pData, static_cast<size_t>(image.LineSize) * static_cast<size_t>(image.Ht));
	return m_Motion.data();
}

void CMotionDetector::SetPixelThreshold(int threshold)
{
	m_PixelThreshold = std::clamp(threshold, 0, 255);
}

void CMotionDetector::SetMotionThreshold(double percent)
{
	m_MotionThreshold = std::clamp(percent, 0.0, 100.0);
}

void CMotionDetector::Reset()
{
	m_HavePreviousFrame = false;
	std::fill(m_Motion.begin(), m_Motion.end(), 0);
}

bool CMotionDetector::Run(MDIMAGE *pImg, bool Refresh)
{
/*	for testing - display the shrunk image 
	m_Shrunk.Process(pImg->pData);
	m_Shrunk.DrawOutputOnBuffer(pImg);
	return true;
*/

	// also if motion detect uses a greyscale calculation theres no point shrinking in colour.
	// dont want to draw on the original bitmap, prefer to highlight the squares using the results structure

	if (Refresh){
		MDIMAGE mdi={0};
		m_Shrunk.Process(pImg->pData);
		mdi.Wd = m_Shrunk.GetOutputWidth();
		mdi.LineSize = mdi.Wd * 3;
		mdi.Ht = m_Shrunk.GetOutputHeight();
		mdi.pData = m_Shrunk.GetOutput();
		Process(mdi);
	}

	int cellWidth  = pImg->Wd / m_Columns;
	int cellHeight = pImg->Ht / m_Rows;

	for (int row = 0; row < m_Rows; ++row){
		int yStart = row * cellHeight;
		// Last row gets any remainder.
		int yEnd = (row == m_Rows - 1) ? pImg->Ht : yStart + cellHeight;

		for (int col = 0; col < m_Columns; ++col){
			int motion_val = m_Motion[(row*m_Columns) + col];
			if (motion_val){

				int xStart = col * cellWidth;
				// Last column gets any remainder.
				int xEnd = (col == m_Columns - 1) ? pImg->Wd : xStart + cellWidth;
				for (int y = yStart; y < yEnd; ++y){

					uint8_t* pixel = pImg->pData + y * pImg->LineSize + xStart * 3;
					for (int x = xStart; x < xEnd; ++x){
						*pixel = 255; // set one channel to 255
						pixel += 3;
					}
				}
			}
		}
	}
	return true;
}



#ifdef sssssssssssssss

bool Image_Processing(PROCESSED_FRAME *pPF, int ThreadIndex)
{
	if (ThreadIndex != 0)
		return false;
	// Image processing logic here, eg, object detection.
	// write results to a log file or database.

	for(int i=0; i<pPF->Image.Ht; i++){
		uint8_t *pLine = pPF->Image.pData + (i * pPF->Image.LineSize);
		for(int j=0; j<pPF->Image.Wd; j++){
			uint8_t *r = &pLine[j*pPF->Image.Planes + 0];
			uint8_t *g = &pLine[j*pPF->Image.Planes + 1];
			uint8_t *b = &pLine[j*pPF->Image.Planes + 2];

			// do something with r,g,b
			*r = 255 - *r;	// invert red channel
			*g = 255 - *g;	// invert green channel
			*b = 255 - *b;	// invert blue channel
		}
	}

/*	static int x=0;
	if ((x%50) == 0){
		// testing
		for (int i=0; i<4; i++){
			pPF->ImgProcOut.Pt[i].x = (rand()*pPF->Wd) / RAND_MAX;
			pPF->ImgProcOut.Pt[i].y = (rand()*pPF->Ht) / RAND_MAX;
		}
	}
	x++;*/

	return true;
}

#endif
