#include <string>

#include "include/video_player_windows/messages.h"

using namespace std::string_literals;
const auto kPositionKey = flutter::EncodableValue("position"s);
const auto kTextureIdKey = flutter::EncodableValue("textureId"s);

flutter::EncodableValue PositionMessage::toEncodable() {
  flutter::EncodableMap m;
  m[kTextureIdKey] = flutter::EncodableValue(textureId);
  m[kPositionKey] = flutter::EncodableValue(position);
  return flutter::EncodableValue(m);
}

TextureMessage::TextureMessage(const flutter::EncodableValue &value) {
  if (!std::holds_alternative<flutter::EncodableMap>(value))
    return;
  flutter::EncodableMap map = std::get<flutter::EncodableMap>(value);
  if (map.find(kTextureIdKey) != map.end()) {
    flutter::EncodableValue thing = map[kTextureIdKey];
    if (std::holds_alternative<int32_t>(thing))
      textureId = std::get<int32_t>(thing);
    if (std::holds_alternative<int64_t>(thing))
      textureId = std::get<int64_t>(thing);
  }
}

flutter::EncodableValue TextureMessage::toEncodable() {
  flutter::EncodableMap map;
  map[kTextureIdKey] = flutter::EncodableValue(textureId);
  return flutter::EncodableValue(map);
}

const auto kAssetKey = flutter::EncodableValue("asset"s);
const auto kUriKey = flutter::EncodableValue("uri"s);
const auto kPackageNameKey = flutter::EncodableValue("packageName"s);
const auto kFormatHintKey = flutter::EncodableValue("formatHint"s);
const auto kHttpHeadersKey = flutter::EncodableValue("httpHeaders"s);

CreateMessage::CreateMessage(const flutter::EncodableValue &value) {
  if (!std::holds_alternative<flutter::EncodableMap>(value))
    return;

  flutter::EncodableMap map = std::get<flutter::EncodableMap>(value);
  if (map.find(kAssetKey) != map.end() && std::holds_alternative<std::string>(map[kAssetKey]))
    asset = std::get<std::string>(map[kAssetKey]);
  if (map.find(kUriKey) != map.end() && std::holds_alternative<std::string>(map[kUriKey]))
    uri = std::get<std::string>(map[kUriKey]);
  if (map.find(kPackageNameKey) != map.end() &&
      std::holds_alternative<std::string>(map[kPackageNameKey]))
    packageName = std::get<std::string>(map[kPackageNameKey]);
  if (map.find(kFormatHintKey) != map.end() &&
      std::holds_alternative<std::string>(map[kFormatHintKey]))
    formatHint = std::get<std::string>(map[kFormatHintKey]);
  if (map.find(kHttpHeadersKey) != map.end() &&
      std::holds_alternative<flutter::EncodableMap>(map[kHttpHeadersKey])) {
    flutter::EncodableMap httpHeaders_in = std::get<flutter::EncodableMap>(map[kHttpHeadersKey]);
    flutter::EncodableMap::iterator it;
    std::unordered_map<std::string, std::string> httpHeaders_out;
    for (it = httpHeaders_in.begin(); it != httpHeaders_in.end(); it++) {
      httpHeaders_out[std::get<std::string>(it->first)] = std::get<std::string>(it->second);
    }
    httpHeaders = httpHeaders_out;
  }
}
