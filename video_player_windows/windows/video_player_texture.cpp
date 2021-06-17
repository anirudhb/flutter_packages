#pragma warning(push, 0)
#include <chrono>
#include <iostream>
#include <sstream>

#include "include/video_player_windows/logging.h"
#include "include/video_player_windows/video_player_texture.h"

#include <flutter/standard_method_codec.h>

extern "C" {
#include <libavfilter/buffersink.h>
#include <libavfilter/buffersrc.h>
#include <libavutil/opt.h>
}

#include <ao.h>

#define check_error_or_die(e)                                                                      \
  do {                                                                                             \
    if (e < 0) {                                                                                   \
      char buf[AV_ERROR_MAX_STRING_SIZE];                                                          \
      av_make_error_string(buf, AV_ERROR_MAX_STRING_SIZE, -e);                                     \
      error_log << "av error: " << buf << std::endl;                                               \
    }                                                                                              \
  } while (0)

static const char *filter_descr =
    "atempo=1.0,volume=1.0,aresample=44100,aformat=sample_fmts=u8:channel_layouts=stereo";

void VideoPlayerTexture::InitAsync(const std::string &uri) { init_uri = uri; }

void VideoPlayerTexture::SetStreamHandlerRef(VideoPlayerStreamHandler *handler) {
  fl_stream_handler = handler;
  has_stream_handler = true;
}

void VideoPlayerTexture::Initialize() {
  if (!init_uri.has_value()) {
    fatal_log << "init_uri doesn't have value!" << std::endl;
    std::exit(1);
  }
  /* Suppress logging to errors */
  av_log_set_level(AV_LOG_ERROR);

  const std::string &uri = *init_uri;
  AVDictionary *opts = NULL;
  // 10 seconds
  av_dict_set(&opts, "timeout", "10000000", 0);

  if (avformat_open_input(&cFormatCtx, uri.c_str(), NULL, &opts) != 0) {
    av_dict_free(&opts);
    fatal_log << "Failed to open input" << std::endl;
    std::exit(1);
  }
  av_dict_free(&opts);
  avformat_find_stream_info(cFormatCtx, NULL);
#ifdef LOG_DEBUG
  av_dump_format(cFormatCtx, 0, uri.c_str(), 0);
#endif
  vStream = -1;
  for (int i = 0; i < cFormatCtx->nb_streams; i++) {
    switch (cFormatCtx->streams[i]->codec->codec_type) {
    case AVMEDIA_TYPE_VIDEO:
      vStream = i;
      break;
    case AVMEDIA_TYPE_AUDIO:
      aStream = i;
      break;
    default:
      break;
    }
  }
  if (vStream == -1) {
    fatal_log << "No video streams found in URI " << uri << "!" << std::endl;
    std::exit(1);
  }

  if (aStream != -1) {
    // audio setup
    aCodecCtx = cFormatCtx->streams[aStream]->codec;
    aFrame = av_frame_alloc();
    aFilterFrame = av_frame_alloc();

    aCodecCtx->codec = avcodec_find_decoder(aCodecCtx->codec_id);
    avcodec_open2(aCodecCtx, aCodecCtx->codec, NULL);
    audio_size_micros = av_q2d(cFormatCtx->streams[aStream]->time_base) * 1000000;

    InitFilterGraph();
  }

  vCodecCtxOrig = cFormatCtx->streams[vStream]->codec;
  pts_size_micros = av_q2d(cFormatCtx->streams[vStream]->time_base) * 1000000;

  vCodec = avcodec_find_decoder(vCodecCtxOrig->codec_id);
  if (vCodec == NULL) {
    fatal_log << "No decoder found for codec ID " << vCodecCtxOrig->codec_id << "!" << std::endl;
    std::exit(1);
  }
  info_log << "Video codec name: " << avcodec_get_name(vCodec->id) << std::endl;

  vCodecCtx = avcodec_alloc_context3(vCodec);
  avcodec_copy_context(vCodecCtx, vCodecCtxOrig);
  avcodec_open2(vCodecCtx, vCodec, NULL);
  fps = av_q2d(vCodecCtx->framerate);
  info_log << "FPS: " << fps << std::endl;

  vFrame = av_frame_alloc();
  vFrameRGB = av_frame_alloc();
  bufsize = avpicture_get_size(AV_PIX_FMT_RGBA, vCodecCtx->width, vCodecCtx->height);
  buffer = static_cast<uint8_t *>(av_malloc(bufsize * sizeof(uint8_t)));
  avpicture_fill(reinterpret_cast<AVPicture *>(vFrameRGB), buffer, AV_PIX_FMT_RGBA,
                 vCodecCtx->width, vCodecCtx->height);

  swsCtx = sws_getContext(vCodecCtx->width, vCodecCtx->height, vCodecCtx->pix_fmt, vCodecCtx->width,
                          vCodecCtx->height, AV_PIX_FMT_RGBA, SWS_BILINEAR, NULL, NULL, NULL);

  initialized = true;
  if (has_stream_handler) {
    fl_stream_handler->SignalInitialized();
  }

  InitializeThreads();
}

void VideoPlayerTexture::InitFilterGraph() {
  if (aStream == -1)
    return;
  // setup filter graph
  char args[512];
  const AVFilter *abuffersrc = avfilter_get_by_name("abuffer");
  const AVFilter *abuffersink = avfilter_get_by_name("abuffersink");
  AVFilterInOut *outputs = avfilter_inout_alloc();
  AVFilterInOut *inputs = avfilter_inout_alloc();
  static const enum AVSampleFormat out_sample_fmts[] = {AV_SAMPLE_FMT_U8,
                                                        static_cast<enum AVSampleFormat>(-1)};
  static const int64_t out_channel_layouts[] = {AV_CH_LAYOUT_STEREO, -1};
  static const int out_sample_rates[] = {44100, -1};
  const AVFilterLink *outlink;
  AVRational aTimeBase = cFormatCtx->streams[aStream]->time_base;

  aFilterGraph = avfilter_graph_alloc();
  if (!aCodecCtx->channel_layout) {
    aCodecCtx->channel_layout = av_get_default_channel_layout(aCodecCtx->channels);
  }
  snprintf(args, sizeof(args), "time_base=%d/%d:sample_rate=%d:sample_fmt=%s:channel_layout=0x%llx",
           aTimeBase.num, aTimeBase.den, aCodecCtx->sample_rate,
           av_get_sample_fmt_name(aCodecCtx->sample_fmt), aCodecCtx->channel_layout);
  debug_log << "Args = " << args << std::endl;
  check_error_or_die(
      avfilter_graph_create_filter(&aSrcCtx, abuffersrc, "in", args, NULL, aFilterGraph));
  check_error_or_die(
      avfilter_graph_create_filter(&aSinkCtx, abuffersink, "out", NULL, NULL, aFilterGraph));
  check_error_or_die(
      av_opt_set_int_list(aSinkCtx, "sample_fmts", out_sample_fmts, -1, AV_OPT_SEARCH_CHILDREN));
  check_error_or_die(av_opt_set_int_list(aSinkCtx, "channel_layouts", out_channel_layouts, -1,
                                         AV_OPT_SEARCH_CHILDREN));
  check_error_or_die(
      av_opt_set_int_list(aSinkCtx, "sample_rates", out_sample_rates, -1, AV_OPT_SEARCH_CHILDREN));
  outputs->name = av_strdup("in");
  outputs->filter_ctx = aSrcCtx;
  outputs->pad_idx = 0;
  outputs->next = NULL;

  inputs->name = av_strdup("out");
  inputs->filter_ctx = aSinkCtx;
  inputs->pad_idx = 0;
  inputs->next = NULL;

  check_error_or_die(avfilter_graph_parse_ptr(aFilterGraph, filter_descr, &inputs, &outputs, NULL));
  check_error_or_die(avfilter_graph_config(aFilterGraph, NULL));

  outlink = aSinkCtx->inputs[0];
  av_get_channel_layout_string(args, sizeof(args), -1, outlink->channel_layout);
  debug_log << "output srate=" << outlink->sample_rate << "hz fmt="
            << av_x_if_null(av_get_sample_fmt_name(static_cast<AVSampleFormat>(outlink->format)),
                            "?")
            << " chlayout=" << args << std::endl;

  avfilter_inout_free(&inputs);
  avfilter_inout_free(&outputs);
}

VideoPlayerTexture::~VideoPlayerTexture() {
  stopped = true;
  if (decodeThread.has_value()) {
    decodeThread->join();
  }
  if (audioThread.has_value()) {
    audioThread->join();
  }
  if (frameThread.has_value()) {
    frameThread->join();
    registrar->UnregisterTexture(tid);
  }
  av_free(buffer);
  if (aStream != -1) {
    av_frame_free(&aFrame);
    av_frame_free(&aFilterFrame);
    avfilter_graph_free(&aFilterGraph);
  }
  av_frame_free(&vFrameRGB);
  av_frame_free(&vFrame);
  avcodec_close(vCodecCtx);
  avcodec_close(vCodecCtxOrig);
  avformat_close_input(&cFormatCtx);
}

int64_t VideoPlayerTexture::GetTextureId() { return tid; }

int64_t VideoPlayerTexture::RegisterWithTextureRegistrar(flutter::TextureRegistrar *registrar) {
  using namespace std::placeholders;

  this->registrar = registrar;

  texture_ = std::make_unique<flutter::TextureVariant>(flutter::PixelBufferTexture(
      [this](size_t width, size_t height) -> const FlutterDesktopPixelBuffer * {
        return this->CopyPixelBuffer(width, height);
      }));

  InitializeThreads();

  int64_t tid = registrar->RegisterTexture(texture_.get());
  this->tid = tid;

  return tid;
}

void VideoPlayerTexture::InitializeThreads() {
  if (!decodeThread.has_value())
    decodeThread = std::thread(std::bind(&VideoPlayerTexture::DecodeThreadProc, this));
  if (threads_initialized || !initialized)
    return;
  frameThread = std::thread(std::bind(&VideoPlayerTexture::FrameThreadProc, this));
  if (aStream != -1)
    audioThread = std::thread(std::bind(&VideoPlayerTexture::AudioThreadProc, this));
  threads_initialized = true;
}

void VideoPlayerTexture::SetupEventChannel(flutter::BinaryMessenger *messenger) {
  std::ostringstream chan_name;
  chan_name << "flutter.io/videoPlayer/videoEvents" << tid;
  info_log << "Registering channel with name " << chan_name.str() << std::endl;
  fl_event_channel = std::make_unique<flutter::EventChannel<flutter::EncodableValue>>(
      messenger, chan_name.str(), &flutter::StandardMethodCodec::GetInstance());
  auto handler = std::make_unique<VideoPlayerStreamHandler>(this);
  fl_event_channel->SetStreamHandler(std::move(handler));
}

void VideoPlayerTexture::DecodeThreadProc() {
  while (!init_uri.has_value()) {
    std::this_thread::sleep_for(std::chrono::microseconds(1000));
  }
  Initialize();
  // Determine maximum number of queue items
  // Would like to keep memory usage <=200mb
  size_t max_queue_items = 209715200 / (vCodecCtx->width * vCodecCtx->height * 4);
  size_t max_aqueue_items = 209715200 / (2 * 44100 * 1);
  info_log << "Max queue items: " << max_queue_items << std::endl;
  info_log << "Max audio queue items: " << max_aqueue_items << std::endl;
  while (!stopped) {
    // Check if paused
    if (paused) {
      while (paused && !stopped)
        std::this_thread::sleep_for(std::chrono::microseconds(1000));
      // nothing to do
    }
    std::optional<VideoFrame> frame;
    std::optional<AudioFrame> adFrame;
    while (!frame.has_value() && !adFrame.has_value()) {
      std::tie(done, frame, adFrame) = ReadFrame();
      if (done || stopped)
        goto done;
    }
    if (adFrame.has_value()) {
      while (true) {
        m_audio_frames.lock();
        if (audio_frames.size() <= max_aqueue_items - 1)
          break;
        m_audio_frames.unlock();
        if (stopped)
          goto done;
        std::this_thread::sleep_for(std::chrono::microseconds(100));
      }
      audio_frames.emplace_back(std::move(*adFrame));
      m_audio_frames.unlock();
    }
    if (frame.has_value()) {
      // Wait until there are <=9 frames!!!
      // very important to avoid oom
      while (true) {
        m_video_frames.lock();
        if (video_frames.size() <= max_queue_items - 1)
          break;
        m_video_frames.unlock();
        if (stopped)
          goto done;
        std::this_thread::sleep_for(std::chrono::microseconds(1000));
      }
      SendTimeUpdate(frame->frame_number * 1000 / fps);
      // Add frame
      video_frames.emplace_back(std::move(*frame));
      m_video_frames.unlock();
    }
  }
done:
  return;
}

void VideoPlayerTexture::AudioThreadProc() {
  ao_initialize();
  ao_sample_format format;
  format.byte_format = AO_FMT_NATIVE;
  format.matrix = "L,R";
  format.channels = aCodecCtx->channels;
  format.bits = 8;
  format.rate = 44100;
  ao_device *device = ao_open_live(ao_default_driver_id(), &format, NULL);
  // frame thread should set playback_start
  while (!stopped) {
    // Check if paused
    if (paused) {
      while (paused && !stopped)
        std::this_thread::sleep_for(std::chrono::microseconds(1000));
      // frame thread should reset playback_start
    }
    AudioFrame frame;
    while (true) {
      m_audio_frames.lock();
      if (!audio_frames.empty()) {
        frame = std::move(audio_frames.front());
        audio_frames.pop_front();
        m_audio_frames.unlock();
        break;
      }
      m_audio_frames.unlock();
      if (done || stopped) {
        goto done;
      }
      std::this_thread::sleep_for(std::chrono::microseconds(1000));
      debug_log << "Waiting for audio frame..." << std::endl;
    }
    auto now = std::chrono::system_clock::now();
    auto target = playback_start + std::chrono::microseconds(audio_size_micros * frame.pts);
    if (now < target) {
      debug_log << "Sleeping for "
                << std::chrono::duration_cast<std::chrono::microseconds>(target - now).count()
                << "micros" << std::endl;
      // Sleep in 10000usec chunks
      long long sz = std::chrono::duration_cast<std::chrono::microseconds>(target - now).count();
      constexpr long long chunk_sz = 10000;
      long long sleep_time = chunk_sz;
      for (long long i = 0; i < sz; i += sleep_time) {
        sleep_time = std::min(chunk_sz, sz - i);
        std::this_thread::sleep_for(std::chrono::microseconds(sleep_time));
        /* Check for desync and drop frame if needed
           That is, if the peeked frame's pts is >1s away from the current frame
         */
        if (!audio_frames.empty() &&
            std::abs(audio_frames.front().pts - frame.pts) > (1000000 / pts_size_micros))
          goto desync;
      }
    }
    // Present the frame
    ao_play(device, reinterpret_cast<char *>(frame.data.data()), frame.data.size());
    continue;
  desync:
    warn_log << "Audio got desynced (is the video being seeked backwards?), dropping frame"
             << std::endl;
  }
done:
  ao_close(device);
}

void VideoPlayerTexture::FrameThreadProc() {
  playback_start = std::chrono::system_clock::now();
  while (!stopped) {
    // Check if paused
    if (paused) {
    pause:
      while (paused && !stopped)
        std::this_thread::sleep_for(std::chrono::microseconds(1000));
      // Reset playback start
      auto old_playback_start = playback_start;
      debug_log << "Current video frame pts = " << current_video_frame.pts << std::endl;
      playback_start = std::chrono::system_clock::now() -
                       std::chrono::microseconds(current_video_frame.pts * pts_size_micros);
      debug_log << "old playback start = " << old_playback_start.time_since_epoch().count()
                << " new = " << playback_start.time_since_epoch().count() << std::endl;
    }
    VideoFrame frame;
    bool didBuffer = false;
    while (true) {
      m_video_frames.lock();
      if (!video_frames.empty()) {
        frame = std::move(video_frames.front());
        video_frames.pop_front();
        m_video_frames.unlock();
        break;
      }
      m_video_frames.unlock();
      if (done || stopped) {
        goto done;
      }
      if (paused)
        goto pause;
      std::this_thread::sleep_for(std::chrono::microseconds(1000));
      if (!didBuffer)
        SendBufferingStart();
      didBuffer = true;
      debug_log << "Waiting for video frame..." << std::endl;
    }
    if (didBuffer)
      SendBufferingEnd();
    // We have a frame...
    auto now = std::chrono::system_clock::now();
    auto target = playback_start + std::chrono::microseconds(pts_size_micros * frame.pts);
    if (now < target) {
      debug_log << "Sleeping for "
                << std::chrono::duration_cast<std::chrono::microseconds>(target - now).count()
                << "micros" << std::endl;
      // Sleep in 10000usec chunks
      long long sz = std::chrono::duration_cast<std::chrono::microseconds>(target - now).count();
      constexpr long long chunk_sz = 10000;
      long long sleep_time = chunk_sz;
      for (long long i = 0; i < sz; i += sleep_time) {
        sleep_time = std::min(chunk_sz, sz - i);
        std::this_thread::sleep_for(std::chrono::microseconds(sleep_time));
        /* Check for desync and drop frame if needed
           That is, if the peeked frame's pts is >1s away from the current frame */
        if (!video_frames.empty() &&
            std::abs(video_frames.front().pts - frame.pts) > (1000000 / pts_size_micros))
          goto desync;
      }
    }
    // force destruction
    current_video_frame = VideoFrame();
    current_video_frame = std::move(frame);
    registrar->MarkTextureFrameAvailable(tid);
    continue;
  desync:
    warn_log << "Video got desynced (is the video being seeked backwards?), dropping frame"
             << std::endl;
  }
done:
  SendCompleted();
}

// first is whether done, second is optional frame, third is optional aframe
std::tuple<bool, std::optional<VideoFrame>, std::optional<AudioFrame>>
VideoPlayerTexture::ReadFrame() {
  // TODO: hw decoding
  bool done = false;
  std::optional<VideoFrame> frame;
  std::optional<AudioFrame> adFrame;
  if (av_read_frame(cFormatCtx, &packet) >= 0) {
    if (packet.stream_index == vStream) {
      int e = avcodec_send_packet(vCodecCtx, &packet);
      if (e < 0) {
        char buf[AV_ERROR_MAX_STRING_SIZE];
        av_strerror(-e, buf, AV_ERROR_MAX_STRING_SIZE);
        error_log << "Decode error: " << buf << std::endl;
      }
      e = avcodec_receive_frame(vCodecCtx, vFrame);
      if (e != AVERROR(EAGAIN) && e != AVERROR_EOF) {
        sws_scale(swsCtx, vFrame->data, vFrame->linesize, 0, vCodecCtx->height, vFrameRGB->data,
                  vFrameRGB->linesize);
        // got a frame
        // Can't trust pts_size_micros since it may be warped by playback speed
        int64_t frame_number = vFrame->pts *
                               (av_q2d(cFormatCtx->streams[vStream]->time_base) * 1000000) * fps /
                               1000000;
        frame = VideoFrame(buffer, bufsize, vFrame->pts, frame_number);
      }
    } else if (aStream != -1 && packet.stream_index == aStream) {
      int e = avcodec_send_packet(aCodecCtx, &packet);
      if (e < 0) {
        char buf[AV_ERROR_MAX_STRING_SIZE];
        av_strerror(-e, buf, AV_ERROR_MAX_STRING_SIZE);
        error_log << "Decode error: " << buf << std::endl;
      }
      e = avcodec_receive_frame(aCodecCtx, aFrame);
      if (e != AVERROR(EAGAIN) && e != AVERROR_EOF) {
        av_buffersrc_add_frame(aSrcCtx, aFrame);
        e = av_buffersink_get_frame(aSinkCtx, aFilterFrame);
        if (e != AVERROR(EAGAIN) && e != AVERROR_EOF) {
          int bufsize = aFilterFrame->nb_samples *
                        av_get_channel_layout_nb_channels(aFilterFrame->channel_layout);
          adFrame = AudioFrame(reinterpret_cast<uint8_t *>(aFilterFrame->data[0]), bufsize,
                               aFilterFrame->pkt_pts);
          av_frame_unref(aFilterFrame);
        }
      }
    }
    av_free_packet(&packet);
  } else {
    info_log << "Done with video" << std::endl;
    done = true;
  }
  return std::make_tuple(done, frame, adFrame);
}

void VideoPlayerTexture::SendTimeUpdate(int64_t millis) {
  if (!fl_event_sink)
    return;
  flutter::EncodableMap m;
  // XXX: send buffering update
  flutter::EncodableList values = {
      flutter::EncodableValue(flutter::EncodableList{
          flutter::EncodableValue(0),
          flutter::EncodableValue(millis),
      }),
  };
  m[flutter::EncodableValue("event")] = flutter::EncodableValue("bufferingUpdate");
  m[flutter::EncodableValue("values")] = flutter::EncodableValue(values);
  fl_event_sink->Success(flutter::EncodableValue(m));
}

void VideoPlayerTexture::SendBufferingStart() {
  if (!fl_event_sink)
    return;
  flutter::EncodableMap m;
  m[flutter::EncodableValue("event")] = flutter::EncodableValue("bufferingStart");
  fl_event_sink->Success(flutter::EncodableValue(m));
}

void VideoPlayerTexture::SendBufferingEnd() {
  if (!fl_event_sink)
    return;
  flutter::EncodableMap m;
  m[flutter::EncodableValue("event")] = flutter::EncodableValue("bufferingEnd");
  fl_event_sink->Success(flutter::EncodableValue(m));
}

void VideoPlayerTexture::SendCompleted() {
  if (!fl_event_sink)
    return;
  info_log << "Sending completed event" << std::endl;
  flutter::EncodableMap m;
  m[flutter::EncodableValue("event")] = flutter::EncodableValue("completed");
  fl_event_sink->Success(flutter::EncodableValue(m));
}

int64_t VideoPlayerTexture::GetPosition() { return current_video_frame.frame_number * 1000 / fps; }
void VideoPlayerTexture::Pause() { paused = true; }
void VideoPlayerTexture::Play() { paused = false; }
void VideoPlayerTexture::Seek(int64_t millis) {
  bool wasPlaying = !paused;
  Pause();
  m_video_frames.lock();
  m_audio_frames.lock();
  // Can't trust pts_size_micros since it may be warped by playback speed
  int64_t target_pts = millis * 1000 / (av_q2d(cFormatCtx->streams[vStream]->time_base) * 1000000);
  // 1 second
  int64_t vTolerance = 1000000 / (av_q2d(cFormatCtx->streams[vStream]->time_base) * 1000000);
  bool isBackwards = target_pts < current_video_frame.pts;
  debug_log << "Vpts tolerance for seeking = " << vTolerance << ", target vpts = " << target_pts
            << std::endl;
  target_pts = std::max(target_pts, static_cast<int64_t>(0));
  av_seek_frame(cFormatCtx, vStream, target_pts | AVSEEK_FORCE, AVSEEK_FLAG_BACKWARD);
  if (aStream != -1) {
    avcodec_flush_buffers(aCodecCtx);
    // Frames are stale
    while (!audio_frames.empty())
      audio_frames.pop_front();
  }
  avcodec_flush_buffers(vCodecCtx);
  // Frames are stale
  while (!video_frames.empty())
    video_frames.pop_front();
  current_video_frame.pts = target_pts;
  std::optional<VideoFrame> vf;
  std::optional<AudioFrame> af;
  while (true) {
    std::optional<VideoFrame> vf2;
    std::optional<AudioFrame> af2;
    std::tie(done, vf2, af2) = ReadFrame();
    if (vf2.has_value())
      vf = std::move(vf2);
    if (af2.has_value())
      af = std::move(af2);
    // NOTE: even when seeking backwards, the best we can do is get a frame after our target
    if (done || (vf.has_value() && (aStream == -1 || af.has_value()) && vf->pts >= target_pts))
      break;
    if (vf.has_value() && (aStream == -1 || af.has_value()))
      debug_log << "Skipping frame with pts=" << vf->pts << ", target=" << target_pts
                << ", tolerance=" << vTolerance << ", backwards=" << isBackwards << std::endl;
  }
  current_video_frame = std::move(*vf);
  if (aStream != -1)
    current_audio_frame = std::move(*af);
  debug_log << "current_video_frame pts = " << current_video_frame.pts << ", target=" << target_pts
            << std::endl;
  m_video_frames.unlock();
  m_audio_frames.unlock();
  if (wasPlaying)
    Play();
}
void VideoPlayerTexture::SetVolume(double volume2) {
  volume = volume2;
  if (aStream == -1)
    return;
  char buf[64];
  snprintf(buf, sizeof(buf), "%.3f", volume2);
  avfilter_graph_send_command(aFilterGraph, "volume", "volume", buf, NULL, 0, 0);
}

const FlutterDesktopPixelBuffer *VideoPlayerTexture::CopyPixelBuffer(size_t width, size_t height) {
  // Forces destructor to be called, so that memory doesn't leak
  current_video_frame2 = VideoFrame();
  current_video_frame2 = std::move(current_video_frame);
  fl_buffer.buffer = current_video_frame2.data.data();
  fl_buffer.width = vCodecCtx->width;
  fl_buffer.height = vCodecCtx->height;
  return &fl_buffer;
}
#pragma warning(pop)