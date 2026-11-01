#include "webgpu/screenshot_pixels.hpp"

#include <gtest/gtest.h>

#include <array>

TEST(ScreenshotPixels, RemovesRowPaddingWithoutChangingRgba) {
  const std::array<uint8_t, 24> pixels{
      1, 2, 3, 4, 5, 6, 7, 8, 99, 99, 99, 99, 9, 10, 11, 12, 13, 14, 15, 16, 99, 99, 99, 99,
  };
  const std::vector<uint8_t> expected{1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16};
  EXPECT_EQ(aurora::screenshot::detail::unpack_rgba(pixels.data(), 2, 2, 12, false), expected);
}

TEST(ScreenshotPixels, SwapsBlueRedAndPreservesAlphaAcrossPaddedRows) {
  const std::array<uint8_t, 16> pixels{1, 2, 3, 4, 99, 99, 99, 99, 5, 6, 7, 8, 99, 99, 99, 99};
  const std::vector<uint8_t> expected{3, 2, 1, 4, 7, 6, 5, 8};
  EXPECT_EQ(aurora::screenshot::detail::unpack_rgba(pixels.data(), 1, 2, 8, true), expected);
}
