# flutter_packages

Various flutter packages.

## video_player_windows

Windows implementation of the video_player native protocol.
Uses libffmpeg (libav & friends) for video playback, and libao for audio playback.

Things to do:

- [x] Separate presentation threads
- [x] Buffer frames (up to 200mb memory usage)
- [ ] hardware decoding
- [ ] Setup ffmpeg zip download and remove hardcoded paths
