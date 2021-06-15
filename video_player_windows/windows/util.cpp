#include <iostream>
#include <sstream>

#include "include/video_player_windows/logging.h"
#include "include/video_player_windows/util.h"

std::string DebugPrintValue(const flutter::EncodableValue &value, int indent) {
  std::ostringstream out;
  for (int i = 0; i < indent; i++)
    out << " ";
  // null, bool, int, double, string, map
  if (std::holds_alternative<std::monostate>(value)) {
    out << "null" << std::endl;
  } else if (std::holds_alternative<bool>(value)) {
    out << std::get<bool>(value) << std::endl;
  } else if (std::holds_alternative<int32_t>(value)) {
    out << std::get<int32_t>(value) << std::endl;
  } else if (std::holds_alternative<int64_t>(value)) {
    out << std::get<int64_t>(value) << std::endl;
  } else if (std::holds_alternative<double>(value)) {
    out << std::get<double>(value) << std::endl;
  } else if (std::holds_alternative<std::string>(value)) {
    out << "string: " << std::get<std::string>(value) << std::endl;
  } else if (std::holds_alternative<flutter::EncodableMap>(value)) {
    auto m = std::get<flutter::EncodableMap>(value);
    flutter::EncodableMap::iterator it;
    for (it = m.begin(); it != m.end(); it++) {
      out << "key:" << std::endl;
      out << DebugPrintValue(it->first, indent + 2);
      out << "value:" << std::endl;
      out << DebugPrintValue(it->second, indent + 2);
    }
  }
  return out.str();
}

flutter::EncodableValue WrapResult(flutter::EncodableValue res) {
  flutter::EncodableMap m;
  m[flutter::EncodableValue("result")] = std::move(res);
  m[flutter::EncodableValue("error")] = flutter::EncodableValue(std::monostate());
  flutter::EncodableValue v = flutter::EncodableValue(std::move(m));
  return v;
}

flutter::EncodableValue WrapError(flutter::EncodableValue err) {
  flutter::EncodableMap m;
  m[flutter::EncodableValue("result")] = flutter::EncodableValue(std::monostate());
  m[flutter::EncodableValue("error")] = std::move(err);
  flutter::EncodableValue v = flutter::EncodableValue(std::move(m));
  return v;
}
