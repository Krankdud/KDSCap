# KDSCap

WIP recording software for Loopy's [Nintendo DS USB Video Capture Board](http://3dscapture.com/ds/)
mod.

### Features
* Uses SDL2 for displaying the video from the DS.
* Nearest-neighbor filtering when scaling the window.
* Currently using WinUSB for interfacing with the device, planning on looking at
  crossplatform alternatives in the future.
* Working on using FFMpeg to encode video and audio.

### Hotkeys
* 1, 2, 3, 4 - Scale window
* M - Switch between vertical, horizontal, GBA top screen, GBA bottom screen modes