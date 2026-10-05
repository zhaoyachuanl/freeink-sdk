#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace freeink::readpico {
constexpr uint16_t WIDTH = 1216;
constexpr uint16_t HEIGHT = 684;
constexpr uint32_t BW_BYTES = uint32_t(WIDTH / 8) * HEIGHT;
constexpr uint32_t GRAY_BYTES = uint32_t(WIDTH / 2) * HEIGHT;

enum class TouchRegion : uint8_t { Invalid, Screen, Back, Next, Previous };

// Classify cover keys before mapping the digitizer's portrait axes to scan axes.
inline TouchRegion mapTouch(uint16_t rawX, uint16_t rawY, uint16_t& panelX, uint16_t& panelY) {
  if (rawY >= 1300) {
    if (rawX >= 480) return TouchRegion::Invalid;
    if (rawX < 160) return TouchRegion::Previous;
    if (rawX < 320) return TouchRegion::Back;
    return TouchRegion::Next;
  }
  if (rawX >= HEIGHT || rawY >= WIDTH) return TouchRegion::Invalid;
  panelX = rawY;
  panelY = HEIGHT - 1 - rawX;
  return TouchRegion::Screen;
}

// FreeInk: MSB-first, 1=white. epdiy: even pixel in LOW nibble, 15=white.
inline constexpr auto BW_EXPANSION = [] {
  std::array<std::array<uint8_t, 4>, 256> table{};
  for (unsigned bits = 0; bits < table.size(); ++bits) {
    for (unsigned pair = 0; pair < 4; ++pair) {
      table[bits][pair] = ((bits & (0x80 >> (pair * 2))) ? 0x0F : 0) | ((bits & (0x40 >> (pair * 2))) ? 0xF0 : 0);
    }
  }
  return table;
}();
inline void expandBw(const uint8_t* source, uint8_t* target, size_t bytes) {
  for (size_t i = 0; i < bytes; ++i) {
    std::memcpy(target + i * 4, BW_EXPANSION[source[i]].data(), 4);
  }
}

// Absolute planes: (LSB, MSB) black=00, dark=10, light=01, white=11.
// Overlay planes: dark=11, light=01; zero bits retain the B/W base.
inline void applyGrayPlane(const uint8_t* source, uint8_t* target, size_t bytes, bool msb, bool absolute) {
  for (size_t i = 0; i < bytes; ++i) {
    if (!absolute && source[i] == 0) continue;
    for (unsigned pixel = 0; pixel < 8; ++pixel) {
      const unsigned shift = 4 * (pixel % 2);
      uint8_t& pair = target[i * 4 + pixel / 2];
      const uint8_t previous = (pair >> shift) & 15;
      const bool bit = (source[i] & (0x80 >> pixel)) != 0;
      uint8_t value = previous;
      if (absolute) {
        value = msb ? uint8_t((previous + (bit ? 2 : 0)) * 5) : uint8_t(bit);
      } else if (bit) {
        value = msb && previous != 5 ? 10 : 5;
      }
      pair = (pair & ~(15 << shift)) | (value << shift);
    }
  }
}
}  // namespace freeink::readpico
