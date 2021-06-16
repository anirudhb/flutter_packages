#pragma once
#include <flutter/event_channel.h>

class VideoPlayerTexture;

class VideoPlayerStreamHandler : public flutter::StreamHandler<flutter::EncodableValue> {
public:
  VideoPlayerStreamHandler(VideoPlayerTexture *texture2) : texture(texture2) {
    SetTextureHandlerRef();
  };

private:
  VideoPlayerTexture *texture;
  bool signal_latch = false;

  std::unique_ptr<flutter::StreamHandlerError<flutter::EncodableValue>>
  OnListenInternal(const flutter::EncodableValue *arguments,
                   std::unique_ptr<flutter::EventSink<flutter::EncodableValue>> &&events) override;

  std::unique_ptr<flutter::StreamHandlerError<flutter::EncodableValue>>
  OnCancelInternal(const flutter::EncodableValue *arguments) override;

  void SetTextureHandlerRef();
  void SignalInitialized();
  flutter::EncodableValue ConstructInitialized();

  friend class VideoPlayerTexture;
};