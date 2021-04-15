#pragma warning(push, 0)
#include <chrono>
#include <iostream>
#include <sstream>

#include "include/video_player_windows/video_player_texture.h"

#include <flutter/standard_method_codec.h>

#include <ao.h>

VideoPlayerTexture::VideoPlayerTexture(const std::string &uri) {
  av_log_set_level(AV_LOG_VERBOSE);
  av_log_set_callback(av_log_default_callback);

  AVDictionary *opts = NULL;
  // 10 seconds
  av_dict_set(&opts, "timeout", "10000000", 0);

  if (avformat_open_input(&cFormatCtx, uri.c_str(), NULL, &opts) != 0) {
    av_dict_free(&opts);
    std::cerr << "Failed to open input" << std::endl;
    std::exit(1);
  }
  av_dict_free(&opts);
  avformat_find_stream_info(cFormatCtx, NULL);
  // debug
  av_dump_format(cFormatCtx, 0, uri.c_str(), 0);
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
    std::cerr << "No video streams found in URI " << uri << "!" << std::endl;
    std::exit(1);
  }

  if (aStream != -1) {
    // audio setup
    aCodecCtx = cFormatCtx->streams[aStream]->codec;
    aFrame = av_frame_alloc();

    aCodecCtx->codec = avcodec_find_decoder(aCodecCtx->codec_id);
    avcodec_open2(aCodecCtx, aCodecCtx->codec, NULL);
    audio_size_micros = av_q2d(cFormatCtx->streams[aStream]->time_base) * 1000000;

    swrCtx = swr_alloc_set_opts(NULL, aCodecCtx->channel_layout, AV_SAMPLE_FMT_U8,
                                aCodecCtx->sample_rate, aCodecCtx->channel_layout,
                                aCodecCtx->sample_fmt, aCodecCtx->sample_rate, 0, NULL);
    swr_init(swrCtx);
  }

  vCodecCtxOrig = cFormatCtx->streams[vStream]->codec;
  pts_size_micros = av_q2d(cFormatCtx->streams[vStream]->time_base) * 1000000;

  vCodec = avcodec_find_decoder(vCodecCtxOrig->codec_id);
  if (vCodec == NULL) {
    std::cerr << "No decoder found for codec ID " << vCodecCtxOrig->codec_id << "!" << std::endl;
    std::exit(1);
  }
  std::cerr << "Video codec name = " << avcodec_get_name(vCodec->id) << std::endl;

  vCodecCtx = avcodec_alloc_context3(vCodec);
  avcodec_copy_context(vCodecCtx, vCodecCtxOrig);
  avcodec_open2(vCodecCtx, vCodec, NULL);
  fps = av_q2d(vCodecCtx->framerate);
  std::cerr << "fps=" << fps << std::endl;

  vFrame = av_frame_alloc();
  vFrameRGB = av_frame_alloc();
  bufsize = avpicture_get_size(AV_PIX_FMT_RGBA, vCodecCtx->width, vCodecCtx->height);
  buffer = static_cast<uint8_t *>(av_malloc(bufsize * sizeof(uint8_t)));
  avpicture_fill(reinterpret_cast<AVPicture *>(vFrameRGB), buffer, AV_PIX_FMT_RGBA,
                 vCodecCtx->width, vCodecCtx->height);

  swsCtx = sws_getContext(vCodecCtx->width, vCodecCtx->height, vCodecCtx->pix_fmt, vCodecCtx->width,
                          vCodecCtx->height, AV_PIX_FMT_RGBA, SWS_BILINEAR, NULL, NULL, NULL);
}

VideoPlayerTexture::~VideoPlayerTexture() {
  stopped = true;
  if (decodeThread.joinable()) {
    decodeThread.join();
  }
  if (audioThread.joinable()) {
    audioThread.join();
  }
  if (frameThread.joinable()) {
    frameThread.join();
    registrar->UnregisterTexture(tid);
  }
  av_free(buffer);
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

  decodeThread = std::thread(std::bind(&VideoPlayerTexture::DecodeThreadProc, this));
  frameThread = std::thread(std::bind(&VideoPlayerTexture::FrameThreadProc, this));
  if (aStream != -1)
    audioThread = std::thread(std::bind(&VideoPlayerTexture::AudioThreadProc, this));

  int64_t tid = registrar->RegisterTexture(texture_.get());
  // registrar->MarkTextureFrameAvailable(tid);
  this->tid = tid;

  return tid;
}

void VideoPlayerTexture::SetupEventChannel(flutter::BinaryMessenger *messenger) {
  std::ostringstream chan_name;
  chan_name << "flutter.io/videoPlayer/videoEvents" << tid;
  std::cout << "Registering channel with name " << chan_name.str() << std::endl;
  fl_event_channel = std::make_unique<flutter::EventChannel<flutter::EncodableValue>>(
      messenger, chan_name.str(), &flutter::StandardMethodCodec::GetInstance());
  fl_stream_handler = std::make_unique<VideoPlayerStreamHandler>(this);
  fl_event_channel->SetStreamHandler(std::move(fl_stream_handler));
  has_stream_handler = true;
}

void VideoPlayerTexture::DecodeThreadProc() {
  // Determine maximum number of queue items
  // Would like to keep memory usage <=200mb
  size_t max_queue_items = 209715200 / (vCodecCtx->width * vCodecCtx->height * 4);
  size_t max_aqueue_items = 209715200 / (2 * 44100 * 1);
  std::cerr << "Max queue items: " << max_queue_items << std::endl;
  std::cerr << "Max audio queue items: " << max_aqueue_items << std::endl;
  while (!stopped) {
    std::optional<VideoFrame> frame;
    std::optional<AudioFrame> adFrame;
    while (!frame.has_value() && !adFrame.has_value()) {
      std::tie(done, frame, adFrame) = ReadFrame();
      if (done)
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
      audio_frames.emplace(std::move(*adFrame));
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
      // Add frame
      video_frames.emplace(std::move(*frame));
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
  // determine proper pts size
  int64_t apts_size_micros = av_q2d(cFormatCtx->streams[aStream]->time_base) * 1000000;
  while (!stopped) {
    AudioFrame frame;
    while (true) {
      m_audio_frames.lock();
      if (!audio_frames.empty()) {
        frame = std::move(audio_frames.front());
        audio_frames.pop();
        m_audio_frames.unlock();
        break;
      }
      m_audio_frames.unlock();
      if (done) {
        goto done;
      }
      std::cerr << "Waiting for audio frame..." << std::endl;
      std::this_thread::sleep_for(std::chrono::microseconds(1000));
    }
    auto now = std::chrono::system_clock::now();
    auto target = playback_start + std::chrono::microseconds(apts_size_micros * frame.pts);
    if (now < target) {
      std::this_thread::sleep_for(target - now);
    }
    // Present the frame
    ao_play(device, reinterpret_cast<char *>(frame.data.data()), frame.data.size());
  }
done:
  ao_close(device);
}

void VideoPlayerTexture::FrameThreadProc() {
  playback_start = std::chrono::system_clock::now();
  while (!stopped) {
    VideoFrame frame;
    while (true) {
      m_video_frames.lock();
      if (!video_frames.empty()) {
        frame = std::move(video_frames.front());
        video_frames.pop();
        m_video_frames.unlock();
        break;
      }
      m_video_frames.unlock();
      if (done) {
        goto done;
      }
      std::this_thread::sleep_for(std::chrono::microseconds(1000));
    }
    // We have a frame...
    auto now = std::chrono::system_clock::now();
    auto target = playback_start + std::chrono::microseconds(pts_size_micros * frame.pts);
    if (now < target) {
      std::this_thread::sleep_for(target - now);
    }
    if (frame.frame_number % fps == 0) {
      int64_t time = frame.frame_number / fps;
      SendTimeUpdate(time);
    }
    // force destruction
    current_video_frame = VideoFrame();
    current_video_frame = std::move(frame);
    registrar->MarkTextureFrameAvailable(tid);
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
        std::cerr << "Decode error!" << std::endl;
        char buf[AV_ERROR_MAX_STRING_SIZE];
        av_strerror(-e, buf, AV_ERROR_MAX_STRING_SIZE);
        std::cerr << buf << std::endl;
      }
      e = avcodec_receive_frame(vCodecCtx, vFrame);
      if (e != AVERROR(EAGAIN) && e != AVERROR_EOF) {
        target_pts = vFrame->pts;
        sws_scale(swsCtx, vFrame->data, vFrame->linesize, 0, vCodecCtx->height, vFrameRGB->data,
                  vFrameRGB->linesize);
        // got a frame
        frame = VideoFrame(buffer, bufsize, vFrame->pts, vCodecCtx->frame_number);
      }
    } else if (aStream != -1 && packet.stream_index == aStream) {
      int e = avcodec_send_packet(aCodecCtx, &packet);
      if (e < 0) {
        std::cerr << "Decode error!" << std::endl;
        char buf[AV_ERROR_MAX_STRING_SIZE];
        av_strerror(-e, buf, AV_ERROR_MAX_STRING_SIZE);
        std::cerr << buf << std::endl;
      }
      e = avcodec_receive_frame(aCodecCtx, aFrame);
      if (e != AVERROR(EAGAIN) && e != AVERROR_EOF) {
        const uint8_t **in = const_cast<const uint8_t **>(aFrame->extended_data);
        uint8_t *out = NULL;
        int out_linesize;
        av_samples_alloc(&out, &out_linesize, 2, 44100, AV_SAMPLE_FMT_U8, 0);
        int ret = swr_convert(swrCtx, &out, 44100, in, aFrame->nb_samples);
        adFrame = AudioFrame(out, ret * aCodecCtx->channels, aFrame->pkt_pts);
      }
    }
    av_free_packet(&packet);
  } else {
    std::cout << "Done with video" << std::endl;
    done = true;
  }
  return std::make_tuple(done, frame, adFrame);
}

void VideoPlayerTexture::SendTimeUpdate(int64_t secs) {
  if (!fl_event_sink)
    return;
  flutter::EncodableMap m;
  std::cerr << "Sending time update secs=" << secs << std::endl;
  // XXX: send buffering update
  flutter::EncodableList values = {
      flutter::EncodableValue(flutter::EncodableList{
          flutter::EncodableValue(0),
          flutter::EncodableValue(secs * 1000),
      }),
  };
  m[flutter::EncodableValue("event")] = flutter::EncodableValue("bufferingUpdate");
  m[flutter::EncodableValue("values")] = flutter::EncodableValue(values);
  fl_event_sink->Success(flutter::EncodableValue(m));
}

void VideoPlayerTexture::SendCompleted() {
  if (!fl_event_sink)
    return;
  std::cerr << "Sending completed event" << std::endl;
  flutter::EncodableMap m;
  m[flutter::EncodableValue("event")] = flutter::EncodableValue("completed");
  fl_event_sink->Success(flutter::EncodableValue(m));
}

int64_t VideoPlayerTexture::GetPosition() { return current_video_frame.frame_number * 1000 / fps; }

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