#include <optional>
#include <string>
#include <unordered_map>

#include <flutter/standard_method_codec.h>

struct PositionMessage {
  int64_t textureId;
  int64_t position;

  PositionMessage() = default;
  PositionMessage(const flutter::EncodableValue &);

  flutter::EncodableValue toEncodable();
};

struct VolumeMessage {
  int64_t textureId;
  double volume = 1;

  VolumeMessage() = default;
  VolumeMessage(const flutter::EncodableValue &);
};

struct PlaybackSpeedMessage {
  int64_t textureId;
  double speed = 1;

  PlaybackSpeedMessage() = default;
  PlaybackSpeedMessage(const flutter::EncodableValue &);
};

struct TextureMessage {
  int64_t textureId;

  TextureMessage() = default;
  TextureMessage(const flutter::EncodableValue &);

  flutter::EncodableValue toEncodable();
};

struct CreateMessage {
  std::optional<std::string> asset;
  std::optional<std::string> uri;
  std::optional<std::string> packageName;
  std::optional<std::string> formatHint;
  std::optional<std::unordered_map<std::string, std::string>> httpHeaders;

  CreateMessage(const flutter::EncodableValue &);
};