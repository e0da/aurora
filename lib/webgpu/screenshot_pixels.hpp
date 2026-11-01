#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <vector>

namespace aurora::screenshot::detail {

inline std::vector<uint8_t> unpack_rgba(const uint8_t* source, uint32_t width, uint32_t height, uint32_t pitch,
                                        bool bgra) {
  const size_t rowBytes = static_cast<size_t>(width) * 4;
  std::vector<uint8_t> result(rowBytes * height);
  for (uint32_t y = 0; y < height; ++y) {
    auto* row = result.data() + rowBytes * y;
    std::memcpy(row, source + static_cast<size_t>(pitch) * y, rowBytes);
    if (bgra) {
      for (uint32_t x = 0; x < width; ++x) {
        const uint8_t blue = row[x * 4];
        row[x * 4] = row[x * 4 + 2];
        row[x * 4 + 2] = blue;
      }
    }
  }
  return result;
}

} // namespace aurora::screenshot::detail
