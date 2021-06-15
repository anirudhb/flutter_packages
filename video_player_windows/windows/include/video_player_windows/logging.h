#pragma once
#include <iostream>

namespace logging {

namespace {
struct __null_stream {
  template <typename T> __null_stream &operator<<(T const &) { return *this; }
  __null_stream &operator<<(std::ostream &(*pf)(std::ostream &)) { return *this; }
};

template <const int color> struct __color_stream {
  __color_stream &start() {
    std::cerr << "\x1b[38;5;" << color << "m" << std::flush;
    return *this;
  }
  template <typename T> __color_stream &operator<<(T const &x) {
    std::cerr << x;
    return *this;
  }
  __color_stream &operator<<(std::ostream &(*pf)(std::ostream &)) {
    pf(std::cerr);
    if (pf == std::endl<char, std::char_traits<char>>) {
      // remove color highlighting
      std::cerr << "\x1b[m" << std::flush;
    }
    return *this;
  }
};
} // namespace

__null_stream __null_stream_singleton;
__color_stream<8> __debug_stream;
__color_stream<14> __info_stream;
__color_stream<11> __warn_stream;
__color_stream<9> __error_stream;

constexpr std::string_view __prepped_basename(const char *fname) {
  std::string_view fname2(fname);
  return fname2.substr(fname2.find_last_of("\\") + 1);
}

#define __prelude_prepped_stream2(STREAM, TAG)                                                     \
  ((STREAM) << "[" << TAG << "] " << logging::__prepped_basename(__FILE__) << ":" << __LINE__      \
            << ":" << __FUNCTION__ << " ")

//#define LOG_DEBUG
#define LOG_INFO

#ifdef LOG_DEBUG
#define debug_log __prelude_prepped_stream2(logging::__debug_stream.start(), "DEBUG")
#else
#define debug_log logging::__null_stream_singleton
#endif
#ifdef LOG_INFO
#define info_log __prelude_prepped_stream2(logging::__info_stream.start(), "INFO ")
#else
#define info_log logging::__null_stream_singleton
#endif
#ifndef LOG_NO_WARN
#define warn_log __prelude_prepped_stream2(logging::__warn_stream.start(), "WARN ")
#else
#define warn_log logging::__null_stream_singleton
#endif
#define error_log __prelude_prepped_stream2(logging::__error_stream.start(), "ERROR")
#define fatal_log __prelude_prepped_stream2(logging::__error_stream.start(), "FATAL")

} // namespace logging