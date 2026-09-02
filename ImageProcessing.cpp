


#include "pch.h"
#include "ImageProcessing.h"
//#include "ObjectDetection.h"

#include <cstdint>
#include <algorithm>
#include <cstring>

// so we don't get min and min macros from the windows headers
#undef max
#undef min


void CMotionDetector::Initialise(int rows, int columns, int verticalSample, int horizontalSample)
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
}

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
		m_PreviousFrame.resize(static_cast<size_t>(image.LineSize) * static_cast<size_t>(image.Ht));
		m_PreviousWd        = image.Wd;
		m_PreviousHt        = image.Ht;
		m_PreviousLineSize  = image.LineSize;
		m_HavePreviousFrame = false;
	}

	if (!m_HavePreviousFrame)
	{
		// First frame - There is nothing to compare it against.
		std::memcpy(m_PreviousFrame.data(), image.pData, static_cast<size_t>(image.LineSize) * static_cast<size_t>(image.Ht));
		std::fill(m_Motion.begin(), m_Motion.end(), 0);
		m_HavePreviousFrame = true;
		return m_Motion.data();
	}

	// Clear output
	std::fill(m_Motion.begin(), m_Motion.end(), 0);

	// Cell dimensions
	const int cellWidth  = image.Wd / m_Columns;
	const int cellHeight = image.Ht / m_Rows;

	// Process cells
	for (int row = 0; row < m_Rows; ++row)
	{
		const int yStart = row * cellHeight;
		const int yEnd = (row == m_Rows - 1) ? image.Ht : yStart + cellHeight;

		for (int col = 0; col < m_Columns; ++col)
		{
			const int xStart = col * cellWidth;
			const int xEnd = (col == m_Columns - 1) ? image.Wd : xStart + cellWidth;
			const int cellWidthActual = xEnd - xStart;
			const int cellHeightActual = yEnd - yStart;

			// Because we are sampling the image, the number of pixels actually examined is smaller than the physical cell size.
			const int sampledWidth = (cellWidthActual + m_HorizontalSample - 1) / m_HorizontalSample;
			const int sampledHeight = (cellHeightActual + m_VerticalSample - 1) / m_VerticalSample;

			const int totalSampledPixels = sampledWidth * sampledHeight;
			const double requiredPixels = totalSampledPixels * (m_MotionThreshold / 100.0);

			int changedPixels = 0;

			// Sample lines vertically
			for (int y = yStart; y < yEnd; y += m_VerticalSample)
			{
				const uint8_t* current = image.pData + static_cast<size_t>(y) * image.LineSize + static_cast<size_t>(xStart) * 3;
				const uint8_t* previous = m_PreviousFrame.data() + static_cast<size_t>(y) * m_PreviousLineSize + static_cast<size_t>(xStart) * 3;

				// Sample pixels horizontally
				for (int x = xStart; x < xEnd; x += m_HorizontalSample)
				{
					// RGB24 -> simple brightness value.
					// We don't actually need to calculate an accurate luminance value. An average is sufficient for basic motion detection.
					const int currentGray  = (current[0] + current[1] + current[2]) / 3;
					const int previousGray = (previous[0] + previous[1] + previous[2]) / 3;
					const int difference   = std::abs(currentGray - previousGray);

					/*
					int dr = std::abs(current[0] - previous[0]);
					int dg = std::abs(current[1] - previous[1]);
					int db = std::abs(current[2] - previous[2]);

					// Use the largest RGB difference.
					//const int difference = std::max({ dr, dg, db });
					int difference = std::max(dr, dg);
					difference = std::max(difference, db);
					*/                    

					if (difference >= m_PixelThreshold)
					{
						++changedPixels;
						// Enough changed pixels to classify this cell as containing motion.
						if (changedPixels >= requiredPixels)
						{
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
	std::memcpy(m_PreviousFrame.data(), image.pData, static_cast<size_t>(image.LineSize) * static_cast<size_t>(image.Ht));
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

bool CMotionDetector::Run(MDIMAGE *pImg)
{
	const uint8_t* motion = Process(*pImg);
	const int cellWidth  = pImg->Wd / m_Columns;
	const int cellHeight = pImg->Ht / m_Rows;

	for (int row = 0; row < m_Rows; ++row)
	{
		const int yStart = row * cellHeight;
		// Last row gets any remainder.
		const int yEnd = (row == m_Rows - 1) ? pImg->Ht : yStart + cellHeight;

		for (int col = 0; col < m_Columns; ++col)
		{
			int motion_val = motion[(row*m_Columns) + col];

			if (motion_val){

				const int xStart = col * cellWidth;
				// Last column gets any remainder.
				const int xEnd = (col == m_Columns - 1) ? pImg->Wd : xStart + cellWidth;

				for (int y = yStart; y < yEnd; ++y)
				{
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

/***************************************************************
todo - DONT SPEND LONG here.

* try add some blur, how? dont want to blur the display bitmap, need another copy.
   or chatGPT suggests, temporal smoothing. Ie, motion needed for more than one frame before its considered as motion.

* motion detection should not be used on every frame.
 
* is the greyscale or colour test best in Process(..) ?

* adjust/tune
 	int    m_PixelThreshold  = 30;
	double m_MotionThreshold = 10.0;

****************************************************************/


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
