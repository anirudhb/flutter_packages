import 'package:flutter/material.dart';
import 'dart:async';

import 'package:flutter/services.dart';
import 'package:video_player/video_player.dart';
// import 'package:video_player_windows/video_player_windows.dart';

void main() {
  runApp(MyApp());
}

class MyApp extends StatefulWidget {
  @override
  _MyAppState createState() => _MyAppState();
}

class _MyAppState extends State<MyApp> {
  bool _videoDisposed = true;
  VideoPlayerController? _controller;
  GlobalKey<ScaffoldState> _scaffoldKey = GlobalKey();
  // String? currentVideoUrl;
  List<List<String>> videos = [
    [
      "Big Buck Bunny 10s 2MB H264",
      "https://test-videos.co.uk/vids/bigbuckbunny/mp4/h264/1080/Big_Buck_Bunny_1080_10s_2MB.mp4"
    ],
    [
      "Big Buck Bunny 10s 10MB H264",
      "https://test-videos.co.uk/vids/bigbuckbunny/mp4/h264/1080/Big_Buck_Bunny_1080_10s_10MB.mp4"
    ],
    [
      "Big Buck Bunny 10s 2MB AV1",
      "https://test-videos.co.uk/vids/bigbuckbunny/mp4/av1/1080/Big_Buck_Bunny_1080_10s_2MB.mp4"
    ],
    [
      "Big Buck Bunny 10s 30MB AV1",
      "https://test-videos.co.uk/vids/bigbuckbunny/mp4/av1/1080/Big_Buck_Bunny_1080_10s_30MB.mp4"
    ],
    [
      "Big Buck Bunny (mp4)",
      "http://commondatastorage.googleapis.com/gtv-videos-bucket/sample/BigBuckBunny.mp4"
    ],
    [
      "Sintel (HLS)",
      "https://multiplatform-f.akamaihd.net/i/multi/april11/sintel/sintel-hd_,512x288_450_b,640x360_700_b,768x432_1000_b,1024x576_1400_m,.mp4.csmil/master.m3u8"
    ],
  ];

  @override
  void initState() {
    super.initState();
  }

  void _initializeVideo(String url) {
    _controller = VideoPlayerController.network(url);
    _controller!.initialize();
    _controller!.play();
    setState(() {
      _videoDisposed = false;
    });
    Navigator.of(_scaffoldKey.currentContext!).pop();
  }

  void _disposeVideo() {
    setState(() {
      _videoDisposed = true;
    });
    _controller?.dispose();
    Navigator.of(_scaffoldKey.currentContext!).pop();
  }

  void _seekTo3Seconds() {
    _controller?.seekTo(Duration(seconds: 3));
  }

  @override
  Widget build(BuildContext context) {
    return MaterialApp(
      home: Scaffold(
        key: _scaffoldKey,
        appBar: AppBar(
          title: !_videoDisposed
              ? ValueListenableBuilder<VideoPlayerValue>(
                  valueListenable: _controller!,
                  builder: (context, value, _) => Text(
                    "Video is ${value.position.inSeconds} / ${value.duration.inSeconds}",
                  ),
                )
              : const Text("Video not initialized"),
        ),
        body: Center(
          child: _videoDisposed
              ? Text("Video is disposed")
              : VideoPlayer(_controller!),
        ),
        drawer: Drawer(
          child: ListView(
            children: _videoDisposed
                ? [
                    for (final thing in videos)
                      ListTile(
                        title: Text("Initialize: ${thing[0]}"),
                        onTap: () => _initializeVideo(thing[1]),
                      ),
                  ]
                : [
                    ListTile(
                      title: Text("Seek to 3s"),
                      onTap: _seekTo3Seconds,
                    ),
                    ListTile(
                      title: Text("Dispose video"),
                      onTap: _disposeVideo,
                    ),
                  ],
          ),
        ),
      ),
    );
  }

  @override
  void dispose() {
    super.dispose();
    _controller?.dispose();
  }
}
