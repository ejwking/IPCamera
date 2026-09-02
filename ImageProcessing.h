

#pragma once

#include <vector>


//extern bool Image_Processing(PROCESSED_FRAME *pImg, int ThreadIndex);


struct MDIMAGE
{
	uint8_t	*pData=nullptr;
	int      Wd=0, Ht=0;
	int      Planes=0, LineSize=0, Padding=0;
};

class CMotionDetector
{
public:
	// verticalSample and horizontalSample, eg: 1 = process every line, 2 = every second line, 3 = every third line, etc.
	void Initialise(int rows, int columns, int verticalSample = 1, int horizontalSample = 1);
	bool Run(MDIMAGE *pImg);
	int GetRows() const
	{
		return m_Rows;
	}
	int GetColumns() const
	{
		return m_Columns;
	}

	// Pixel brightness difference required before a pixel is considered changed.
	// 0 = very sensitive, 255 = insensitive
	void SetPixelThreshold(int threshold);

	// Percentage of sampled pixels in a cell that must have changed before the cell is considered to contain motion.
	// Example: 2.0 = 2% of sampled pixels must change.
	void SetMotionThreshold(double percent);

	// Forget the previous frame.
	void Reset();

private:
	int m_Rows, m_Columns;
	int m_VerticalSample, m_HorizontalSample;
	int m_PreviousWd = 0, m_PreviousHt = 0, m_PreviousLineSize = 0;
	bool m_HavePreviousFrame = false;

	int    m_PixelThreshold  = 30;
	double m_MotionThreshold = 10.0;

	// Previous image has exactly the same dimensions and LineSize as the source image.
	std::vector<uint8_t> m_PreviousFrame;
	// One byte per cell.
	std::vector<uint8_t> m_Motion;

	// Process a frame. Returns an array of rows * columns bytes: 0 = no motion, 1 = motion
	// The returned pointer remains valid until the next call.
	const uint8_t* Process(const MDIMAGE& image);
};

