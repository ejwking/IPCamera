

#pragma once

#include <vector>


//extern bool Image_Processing(PROCESSED_FRAME *pImg, int ThreadIndex);

struct MDIMAGE
{
	uint8_t	*pData=nullptr;
	int      Wd, Ht;
	int      Planes, LineSize, Padding;
};


class CBitmapShrinker
{
public:
	~CBitmapShrinker();
	void Initialise(int width, int height, int factor);

	// Returns pointer to resized bitmap (RGB)
	uint8_t* GetOutput() const { return m_out; }
	int GetOutputWidth()  const { return m_outW; }
	int GetOutputHeight() const { return m_outH; }

	// Perform the shrink
	void Process(const uint8_t* input);
	void DrawOutputOnBuffer(MDIMAGE *pBuf);

private:
	uint8_t* m_out = nullptr;
	int m_w=0, m_h=0;
	int m_factor;
	int m_outW=0, m_outH=0;
};



class CMotionDetector
{
public:
	~CMotionDetector()
	{
		if (m_PreviousFrame)
			delete[] m_PreviousFrame;
	}

	// verticalSample and horizontalSample, eg: 1 = process every line, 2 = every second line, 3 = every third line, etc.
	void Initialise(const MDIMAGE& image, int rows, int columns, int BitmapReductionFactor, int verticalSample = 1, int horizontalSample = 1);
	bool Run(MDIMAGE *pImg, bool Refresh, bool &Motion);
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

	int    m_AllChanThreshold    = 60; // r + g + b
	int    m_SingleChanThreshold = 30; // r || g || b
	double m_MotionThreshold     = 30.0; // also linked to grid cell size, because if the cell is larger it will be harder to meet the threshold with the movement of small objects.

	CBitmapShrinker m_Shrunk;

	// Previous image has exactly the same dimensions and LineSize as the source image.
	uint8_t* m_PreviousFrame=nullptr;
	// One byte per cell.
	std::vector<uint8_t> m_Motion;

	// Process a frame. Returns an array of rows * columns bytes: 0 = no motion, 1 = motion
	// The returned pointer remains valid until the next call.
	const uint8_t* Process(const MDIMAGE& image);
};

