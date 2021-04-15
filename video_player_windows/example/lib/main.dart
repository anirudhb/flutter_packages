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

  @override
  void initState() {
    super.initState();
    // _controller.play();
  }

  void _initializeVideo() {
    setState(() {
      _controller = VideoPlayerController.network(
          // "https://test-videos.co.uk/vids/bigbuckbunny/mp4/h264/1080/Big_Buck_Bunny_1080_10s_2MB.mp4");
          // "https://test-videos.co.uk/vids/bigbuckbunny/mp4/h264/1080/Big_Buck_Bunny_1080_10s_10MB.mp4");
          // "https://test-videos.co.uk/vids/bigbuckbunny/mp4/av1/1080/Big_Buck_Bunny_1080_10s_2MB.mp4");
          // "https://test-videos.co.uk/vids/bigbuckbunny/mp4/av1/1080/Big_Buck_Bunny_1080_10s_30MB.mp4");
          // "http://commondatastorage.googleapis.com/gtv-videos-bucket/sample/BigBuckBunny.mp4");
          "https://multiplatform-f.akamaihd.net/i/multi/april11/sintel/sintel-hd_,512x288_450_b,640x360_700_b,768x432_1000_b,1024x576_1400_m,.mp4.csmil/master.m3u8");
      _controller!.initialize();
      _videoDisposed = false;
    });
    Navigator.of(_scaffoldKey.currentContext!).pop();
  }

  void _disposeVideo() {
    setState(() {
      _controller?.dispose();
      _videoDisposed = true;
    });
    Navigator.of(_scaffoldKey.currentContext!).pop();
  }

  @override
  Widget build(BuildContext context) {
    return MaterialApp(
      home: Scaffold(
        key: _scaffoldKey,
        appBar: AppBar(
          title: const Text('Plugin example app'),
        ),
        body: Center(
          child: _videoDisposed
              ? Text("Video is disposed")
              : VideoPlayer(_controller!),
        ),
        drawer: Drawer(
          child: ListView(
            children: [
              ListTile(
                title:
                    Text(_videoDisposed ? "Initialize video" : "Dispose video"),
                onTap: _videoDisposed ? _initializeVideo : _disposeVideo,
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
