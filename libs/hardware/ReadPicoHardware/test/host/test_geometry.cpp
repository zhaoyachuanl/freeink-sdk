#include <ReadPicoGeometry.h>

#include <array>
#include <cassert>
#include <cstdio>

using namespace freeink::readpico;

int main() {
  // Exhaust all source bytes; independently inspect each pixel's nibble.
  for (unsigned bits = 0; bits < 256; ++bits) {
    const uint8_t source = bits;
    std::array<uint8_t, 6> target = {0xA5, 0, 0, 0, 0, 0x5A};
    expandBw(&source, target.data() + 1, 1);
    assert(target.front() == 0xA5 && target.back() == 0x5A);
    for (unsigned pixel = 0; pixel < 8; ++pixel) {
      const unsigned level = (target[1 + pixel / 2] >> (4 * (pixel % 2))) & 15;
      assert(level == ((bits & (0x80 >> pixel)) ? 15 : 0));
    }
  }
  uint16_t x = 0, y = 0;
  // Digitizer corners must match CrossPoint's existing clockwise portrait transform.
  assert(mapTouch(0, 0, x, y) == TouchRegion::Screen && x == 0 && y == 683);
  assert(mapTouch(683, 1215, x, y) == TouchRegion::Screen && x == 1215 && y == 0);
  assert(mapTouch(684, 400, x, y) == TouchRegion::Invalid);
  assert(mapTouch(200, 1216, x, y) == TouchRegion::Invalid);
  assert(mapTouch(80, 1299, x, y) == TouchRegion::Invalid);
  assert(mapTouch(80, 1300, x, y) == TouchRegion::Previous);
  assert(mapTouch(159, 1500, x, y) == TouchRegion::Previous);
  assert(mapTouch(160, 1500, x, y) == TouchRegion::Back);
  assert(mapTouch(319, 1500, x, y) == TouchRegion::Back);
  assert(mapTouch(320, 1500, x, y) == TouchRegion::Next);
  assert(mapTouch(479, 1600, x, y) == TouchRegion::Next);
  assert(mapTouch(480, 1500, x, y) == TouchRegion::Invalid);
  assert(mapTouch(80, 4095, x, y) == TouchRegion::Previous);
  // Independently synthesize all four levels in both renderer encodings.
  for (const bool absolute : {false, true}) {
    std::array<uint8_t, 4> gray = {};
    const uint8_t bw = 0x33;  // black, dark, light, white repeated twice
    const uint8_t lsb = absolute ? 0x55 : 0x44;
    const uint8_t msb = absolute ? 0x33 : 0x66;
    expandBw(&bw, gray.data(), 1);
    applyGrayPlane(&lsb, gray.data(), 1, false, absolute);
    applyGrayPlane(&msb, gray.data(), 1, true, absolute);
    for (unsigned pixel = 0; pixel < 8; ++pixel)
      assert(((gray[pixel / 2] >> (4 * (pixel % 2))) & 15) == 5 * (pixel % 4));
  }
  const uint8_t zero = 0;
  for (bool msb : {false, true}) {
    std::array<uint8_t, 4> gray = {0xA5, 0xF0, 0x38, 0xC6};
    const auto previous = gray;
    applyGrayPlane(&zero, gray.data(), 1, msb, false);
    assert(gray == previous);
    if (!msb) {
      applyGrayPlane(&zero, gray.data(), 1, false, true);
      for (auto value : gray) assert(value == 0);
    }
  }
  static_assert(BW_BYTES == 103968 && GRAY_BYTES == 415872);
  std::puts("Read Pico packing, orientation and cover-key boundaries passed");
}
