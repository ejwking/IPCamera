
#pragma once

#include <string>
#include <memory>


/*
If copying were allowed, this would compile:
Frame a;
Frame b = a;

But what should that mean?

Should it:
Copy the AVFrame?
Share the AVFrame?
Duplicate the image buffer?
Just copy the pointer?

There isn't an obvious correct answer.

Without = delete
The compiler would try to generate copy operations automatically.
Because Frame contains a std::unique_ptr, the copy constructor is actually already deleted by the compiler.
So this wouldn't compile anyway.
So why did I write them?
Simply to make the intention explicit.
When someone reads the class, they immediately know:
"A Frame is a unique owner of an image."
Are they necessary?

Strictly speaking...No.
Because std::unique_ptr already prevents copying.
*/

class Frame
{
public:

    Frame();
    ~Frame();

	// These 2 lines tell the compiler: Do not generate a copy constructor or copy assignment operator for this class. If anyone tries to copy a Frame object, it will result in a compile-time error. 
    Frame(const Frame&) = delete;
    Frame& operator=(const Frame&) = delete;

    int Width() const;
    int Height() const;
    int Stride() const;

    const uint8_t* ScanLine(int y) const;

    bool IsValid() const;

    int PixelFormat() const;

    const char * PixelFormatName() const;

private:

    class Impl;
    std::unique_ptr<Impl> m_Impl;

    friend class Camera;
    friend class ImageConverter;
};



class ImageConverter
{
public:

    ImageConverter();
    ~ImageConverter();

    ImageConverter(const ImageConverter&) = delete;
    ImageConverter& operator=(const ImageConverter&) = delete;

    bool Convert(const Frame& source, Frame& destination);

private:

    class Impl;
    std::unique_ptr<Impl> m_Impl;
};



struct VideoInfo
{
    std::string codecName;
    int width = 0, height = 0;
	int codec_id = 0;
    double fps = 0.0;
};


class Camera
{
public:

	VideoInfo m_VideoInfo;

    Camera();
    ~Camera();

    void GetVideoInfo();
    bool Open(const std::string& url);
    bool Grab();
    const Frame& CurrentFrame() const;
    void Close();

private:

    // The PImpl Idiom (Pointer to IMPLementation) is a technique used for separating implementation from the interface. It minimizes header exposure.
	// Fairly pointless for my little project, but I wanted to try it out. It is a good technique for large projects where you want to hide implementation details and reduce compilation dependencies.
    class Impl;     // forward declaration
    Impl* m_Impl;   // hide impl details
};