#include <vector>

class VideoFrame {
public:
  VideoFrame() = default;
  VideoFrame(uint8_t *data_ptr, size_t data_size, int64_t pts, int frame_number)
      : data(data_ptr, data_ptr + data_size), pts(pts), frame_number(frame_number){};

  int frame_number;
  int64_t pts;
  std::vector<uint8_t> data;
};

class AudioFrame {
public:
  AudioFrame() = default;
  AudioFrame(uint8_t *data_ptr, size_t data_size, int64_t pts)
      : data(data_ptr, data_ptr + data_size), pts(pts){};

  int64_t pts;
  std::vector<uint8_t> data;
};