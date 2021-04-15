#include <iostream>

#include "include/video_player_windows/util.h"

void DebugPrintValue(const flutter::EncodableValue &value, int indent) {
  for (int i = 0; i < indent; i++)
    std::cout << " ";
  // null, bool, int, double, string, map
  if (std::holds_alternative<std::monostate>(value)) {
    std::cout << "null" << std::endl;
  } else if (std::holds_alternative<bool>(value)) {
    std::cout << std::get<bool>(value) << std::endl;
  } else if (std::holds_alternative<int32_t>(value)) {
    std::cout << std::get<int32_t>(value) << std::endl;
  } else if (std::holds_alternative<int64_t>(value)) {
    std::cout << std::get<int64_t>(value) << std::endl;
  } else if (std::holds_alternative<double>(value)) {
    std::cout << std::get<double>(value) << std::endl;
  } else if (std::holds_alternative<std::string>(value)) {
    std::cout << "string: " << std::get<std::string>(value) << std::endl;
  } else if (std::holds_alternative<flutter::EncodableMap>(value)) {
    auto m = std::get<flutter::EncodableMap>(value);
    flutter::EncodableMap::iterator it;
    for (it = m.begin(); it != m.end(); it++) {
      std::cout << "key:" << std::endl;
      DebugPrintValue(it->first, indent + 2);
      std::cout << "value:" << std::endl;
      DebugPrintValue(it->second, indent + 2);
    }
  }
}

flutter::EncodableValue WrapResult(flutter::EncodableValue res) {
  flutter::EncodableMap m;
  m[flutter::EncodableValue("result")] = std::move(res);
  m[flutter::EncodableValue("error")] = flutter::EncodableValue(std::monostate());
  // std::cout << "Wrapped value:" << std::endl;
  flutter::EncodableValue v = flutter::EncodableValue(std::move(m));
  // DebugPrintValue(v);
  return v;
}

flutter::EncodableValue WrapError(flutter::EncodableValue err) {
  flutter::EncodableMap m;
  m[flutter::EncodableValue("result")] = flutter::EncodableValue(std::monostate());
  m[flutter::EncodableValue("error")] = std::move(err);
  // std::cout << "Wrapped value:" << std::endl;
  flutter::EncodableValue v = flutter::EncodableValue(std::move(m));
  // DebugPrintValue(v);
  return v;
}
