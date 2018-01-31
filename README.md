[//]: # (For development of this README.md, use http://markdownlivepreview.com/)

# Intel RealSense TOP

## Development Usage
1. Install the [RealSense SDK](https://github.com/IntelRealSense/librealsense) to "C:\Program Files (x86)\Intel RealSense SDK 2.0"
2. Go to "C:\Program Files (x86)\Intel RealSense SDK 2.0\bin\x64". Copy "LibrealsenseWrapper.dll" and "realsense2.dll" into this repository's "Release\x64" folder.
3. Open the Visual Studio Solution "OpenGLTOP.sln"
4. Hit F5 on your keyboard, which will open the TouchDesigner099 project.

## changelog
* 2018-01-31 No crash/freeze, but the depth doesn't render and there's a memory leak.
* 2018-01-31 TouchDesigner loads dll but freezes. "librealsense::wrong_api_call_sequence_exception"
* 2018-01-29 first commit, but .dll file has errors