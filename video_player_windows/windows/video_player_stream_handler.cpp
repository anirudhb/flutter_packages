#include "include/video_player_windows/video_player_stream_handler.h"

#include "include/video_player_windows/logging.h"
#include "include/video_player_windows/video_player_texture.h"

#include <flutter/encodable_value.h>

flutter::EncodableValue VideoPlayerStreamHandler::ConstructInitialized() {
  flutter::EncodableMap m;
  // Safe to read our ptr since the video is always initialized prior
  m[flutter::EncodableValue("event")] = flutter::EncodableValue("initialized");
  int64_t duration = texture->cFormatCtx->duration * 1000 / AV_TIME_BASE;
  int width = texture->vCodecCtx->width;
  int height = texture->vCodecCtx->height;
  info_log << duration << "ms, " << width << "x" << height << std::endl;
  m[flutter::EncodableValue("duration")] = flutter::EncodableValue(duration);
  m[flutter::EncodableValue("width")] = flutter::EncodableValue(width);
  m[flutter::EncodableValue("height")] = flutter::EncodableValue(height);
  return flutter::EncodableValue(m);
}

std::unique_ptr<flutter::StreamHandlerError<flutter::EncodableValue>>
VideoPlayerStreamHandler::OnListenInternal(
    const flutter::EncodableValue *arguments,
    std::unique_ptr<flutter::EventSink<flutter::EncodableValue>> &&events) {
  texture->fl_event_sink = std::move(events);
  // Send the initialized event
  texture->fl_event_sink->Success(ConstructInitialized());
  // End buffering
  flutter::EncodableMap m;
  m[flutter::EncodableValue("event")] = flutter::EncodableValue("bufferingEnd");
  texture->fl_event_sink->Success(flutter::EncodableValue(m));
  return nullptr;
}

std::unique_ptr<flutter::StreamHandlerError<flutter::EncodableValue>>
VideoPlayerStreamHandler::OnCancelInternal(const flutter::EncodableValue *arguments) {
  return nullptr;
}
