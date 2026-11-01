#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace aurora::screenshot {

struct Result {
  uint32_t width = 0;
  uint32_t height = 0;
  std::vector<uint8_t> rgba;
  std::string error;
};

// Captures the next composed frame. A completed, unpolled capture is still busy.
bool request();
bool poll(Result& result);
// Discards delivery; an outstanding GPU callback retains its own resources.
void cancel();

} // namespace aurora::screenshot
