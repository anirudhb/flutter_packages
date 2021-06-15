#include <flutter/encodable_value.h>

// Debug prints an encodable value
std::string DebugPrintValue(const flutter::EncodableValue &value, int indent = 0);

// Wraps a result into the format expected by pigeon
flutter::EncodableValue WrapResult(flutter::EncodableValue result);

// Wraps an error into the format expected by pigeon
flutter::EncodableValue WrapError(flutter::EncodableValue error);