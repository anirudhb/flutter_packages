#include <atomic>
#include <chrono>
#include <deque>
#include <mutex>
#include <optional>
#include <string>
#include <thread>

#include "frame.h"
#include "video_player_stream_handler.h"

#pragma warning(push, 0)
#ifdef __cplusplus
extern "C" {
#endif
#include <libavcodec/avcodec.h>
#include <libavfilter/avfilter.h>
#include <libavformat/avformat.h>
#include <libswresample/swresample.h>
#include <libswscale/swscale.h>
#ifdef __cplusplus
}
#endif
#pragma warning(pop)

#include <flutter/binary_messenger.h>
#include <flutter/event_channel.h>
#include <flutter/texture_registrar.h>

class VideoPlayerTexture {
public:
  ~VideoPlayerTexture();
  // Initializes the video player asynchronously
  void InitAsync(const std::string &uri);

  // Returns the texture ID
  int64_t RegisterWithTextureRegistrar(flutter::TextureRegistrar *registrar);

  int64_t GetTextureId();

  void SetupEventChannel(flutter::BinaryMessenger *messenger);

  int64_t GetPosition();
  void Play();
  void Pause();
  void Seek(int64_t millis);
  void SetVolume(double volume);

private:
  std::tuple<bool, std::optional<VideoFrame>, std::optional<AudioFrame>> ReadFrame();
  const FlutterDesktopPixelBuffer *CopyPixelBuffer(size_t width, size_t height);
  void DecodeThreadProc();
  void FrameThreadProc();
  void AudioThreadProc();

  void InitFilterGraph();

  void SendTimeUpdate(int64_t secs);
  void SendBufferingStart();
  void SendBufferingEnd();
  void SendCompleted();

  /* Requires that init_uri is valid */
  void Initialize();
  /* Sets threads_initialized, does nothing if set.
     Does nothing if this texture is not initialized.
     Initializes decode_thread regardless since it is responsible for initializing the video. */
  void InitializeThreads();

  void SetStreamHandlerRef(VideoPlayerStreamHandler *handler);

  AVFormatContext *cFormatCtx = NULL;
  AVCodecContext *vCodecCtx = NULL;
  AVCodecContext *vCodecCtxOrig = NULL;
  AVFilterContext *aSinkCtx = NULL;
  AVFilterContext *aSrcCtx = NULL;
  AVFilterGraph *aFilterGraph = NULL;
  AVCodecContext *aCodecCtx = NULL;
  AVCodec *vCodec = NULL;
  int vStream;
  int aStream = -1;
  AVFrame *vFrame = NULL;
  AVFrame *vFrameRGB = NULL;
  AVFrame *aFrame = NULL;
  AVFrame *aFilterFrame = NULL;
  AVPacket packet;
  int frameFinished;
  uint8_t *buffer = NULL;
  uint8_t *render_buffer = NULL;
  int bufsize;
  struct SwsContext *swsCtx = NULL;
  std::atomic<int64_t> pts_size_micros = 0;
  std::atomic<int64_t> audio_size_micros = 0;
  int64_t fps = 1;
  std::chrono::time_point<std::chrono::system_clock> playback_start;

  FlutterDesktopPixelBuffer fl_buffer;
  std::unique_ptr<flutter::TextureVariant> texture_;
  VideoPlayerStreamHandler *fl_stream_handler;
  std::unique_ptr<flutter::EventChannel<flutter::EncodableValue>> fl_event_channel;
  std::unique_ptr<flutter::EventSink<flutter::EncodableValue>> fl_event_sink;
  std::atomic<bool> has_stream_handler;
  std::atomic<bool> done;
  std::atomic<bool> stopped;
  std::atomic<bool> paused;
  std::atomic<double> volume = 1;
  flutter::TextureRegistrar *registrar;
  int64_t tid;
  std::deque<VideoFrame> video_frames;
  std::mutex m_video_frames;
  VideoFrame current_video_frame = VideoFrame();
  // Used by renderer
  VideoFrame current_video_frame2 = VideoFrame();
  std::deque<AudioFrame> audio_frames;
  std::mutex m_audio_frames;
  AudioFrame current_audio_frame;
  std::optional<std::thread> decodeThread;
  std::optional<std::thread> frameThread;
  std::optional<std::thread> audioThread;

  std::atomic<bool> initialized;
  std::atomic<bool> threads_initialized;
  std::optional<std::string> init_uri;

  friend class VideoPlayerStreamHandler;
};