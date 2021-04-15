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
  String _platformVersion = 'Unknown';
  VideoPlayerController _controller = VideoPlayerController.network(
      // "https://test-videos.co.uk/vids/bigbuckbunny/mp4/h264/1080/Big_Buck_Bunny_1080_10s_2MB.mp4");
      // "https://test-videos.co.uk/vids/bigbuckbunny/mp4/h264/1080/Big_Buck_Bunny_1080_10s_10MB.mp4");
      // "https://test-videos.co.uk/vids/bigbuckbunny/mp4/av1/1080/Big_Buck_Bunny_1080_10s_2MB.mp4");
      // "https://test-videos.co.uk/vids/bigbuckbunny/mp4/av1/1080/Big_Buck_Bunny_1080_10s_30MB.mp4");
      // "http://commondatastorage.googleapis.com/gtv-videos-bucket/sample/BigBuckBunny.mp4");
      "https://better-twist.web.app/api/pahevideo.m3u8?url=https%3A%2F%2Fkwik.cx%2Fe%2FmKpt3vpUbeAY");

  @override
  void initState() {
    super.initState();
    _controller.initialize();
    // _controller.play();
  }

  @override
  Widget build(BuildContext context) {
    return MaterialApp(
      home: Scaffold(
        appBar: AppBar(
          title: const Text('Plugin example app'),
        ),
        body: Center(
          child: VideoPlayer(_controller),
        ),
      ),
    );
  }
}
