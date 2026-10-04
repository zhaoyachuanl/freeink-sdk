#include <BoardConfig.h>

#if FREEINK_DRIVER_READPICO
#include <ReadPicoHardware.h>

#include "ReadPicoDriver.h"

namespace freeink {
namespace {
class ReadPicoDriver : public PanelDriver {
 public:
  uint32_t spiHz() const override { return 0; }
  BusyPolarity busyPolarity() const override { return BusyPolarity::ActiveHigh; }
  PanelGeometry geometry() const override {
    return {readpico::WIDTH, readpico::HEIGHT, readpico::WIDTH / 8, readpico::BW_BYTES};
  }
  bool usesExternalBus() const override { return true; }
  void begin(EpdBus&) override { readpico::beginDisplay(); }
  void deepSleep(EpdBus&) override { readpico::sleepDisplay(); }
  void display(EpdBus&, const uint8_t* fb, const uint8_t*, RefreshMode mode, bool turnOff) override {
    readpico::displayBw(fb, mode == RefreshMode::Full, mode == RefreshMode::Fast, turnOff);
  }
  GrayscaleCapabilities grayscaleCapabilities(GrayscaleMode mode) const override {
    return {mode == GrayscaleMode::Overlay ? GrayscaleEncoding::OverlayMasks : GrayscaleEncoding::AbsolutePlanes,
            GrayscaleBase::Combined, true, false, false};
  }
  void displayGrayscaleBase(EpdBus&, const uint8_t* fb, RefreshMode fallback, bool) override {
    readpico::beginGray(fb, false, fallback != RefreshMode::Fast);
  }
  void beginGrayscale(EpdBus&, const uint8_t* fb, GrayscaleMode mode, RefreshMode fallback, bool) override {
    readpico::beginGray(fb, mode != GrayscaleMode::Overlay, fallback != RefreshMode::Fast);
  }
  void copyGrayscaleLsb(EpdBus&, const uint8_t* lsb) override { readpico::uploadGrayPlane(lsb, false); }
  void copyGrayscaleMsb(EpdBus&, const uint8_t* msb) override { readpico::uploadGrayPlane(msb, true); }
  void writeGrayscalePlaneStrip(EpdBus&, GrayPlane plane, const uint8_t* rows, uint16_t yStart,
                                uint16_t numRows) override {
    readpico::uploadGrayRows(rows, plane == GrayPlane::Msb, yStart, numRows);
  }
  void cleanupGrayscaleBuffers(EpdBus&, const uint8_t* bw) override { readpico::cleanupGray(bw); }
  void displayGray(EpdBus&, const uint8_t*, bool turnOff, const unsigned char*, bool) override {
    readpico::displayGray(turnOff);
  }
};
}  // namespace
PanelDriver& readPicoDriver() {
  static ReadPicoDriver driver;
  return driver;
}
}  // namespace freeink
#endif
