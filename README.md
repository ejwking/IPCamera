# IPCamera

Just for fun and learning.
Using FFmpeg library for processing video from my IP cameras.

Objectives/ideas:  
Motion detection, Object detection. Maybe using OpenCV, YOLO (Darknet), or Ollama to run an LLM locally.  
Run on Windows and Linux (RPi).  

               +-------------------+
               |   Camera Library  |
               |-------------------|
               | RTSP              |
               | ONVIF             |
               | Video decoding    |
               | Image processing  |
               | Event recording   |
               +-------------------+
                    ^          ^
                    |          |
          +---------+          +---------+
          |                              |
    Windows GUI (MFC)          Linux GUI (wxWidgets)

