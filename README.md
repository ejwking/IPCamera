# IPCamera

Just for fun and learning.
Using FFmpeg library for processing video from my IP cameras.

Current functionality:  
List of your rtsp url's (or .mp4, .mv4, etc file paths) in text file - config.txt, no limit to the number urls/paths.  
Each video stream is serviced by 2 std::thread's:  
  ...first for FFmpeg grab and decode of each frame.  
  ...second for image processing and related tasks. Currently just doing motion detection, I might add event recording too.  
Windows GUI thread - the video from each stream is displayed in a grid in the window, and motion is highlighted.  


Next ideas:  
Linux GUI using wxWidgets.  
Web server (for Linux), and web interface.  
