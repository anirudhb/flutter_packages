# flutter_packages

Various flutter packages.

## video_player_windows

Windows implementation of the video_player native protocol.
Uses libffmpeg (libav & friends) for video playback, and libao for audio playback.

Playback speed is **not supported**.

Things to do:

- [x] Separate presentation threads
- [x] Buffer frames (up to 200mb memory usage)
- [x] ~~hardware decoding~~ apparently is done by FFmpeg automatically
- [x] Setup ffmpeg zip download and remove hardcoded paths
