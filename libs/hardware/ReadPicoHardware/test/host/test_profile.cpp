#include <BoardConfig.h>

#include <cassert>
#include <cstdio>

int main() {
#if FREEINK_DEVICE_READPICO
  static_assert(FREEINK_MCU_S3 && !FREEINK_MCU_C3);
  static_assert(FREEINK_DRIVER_READPICO && !FREEINK_DRIVER_SSD1677 && !FREEINK_DRIVER_LGFX_EPD);
  static_assert(FREEINK_CAP_TOUCH && FREEINK_SD_SDMMC && FREEINK_FB_PSRAM);
  static_assert(!FREEINK_CAP_IMU && !FREEINK_CAP_RTC && !FREEINK_CAP_FRONTLIGHT);
  static_assert(BoardConfig::MAX_FRAMEBUFFER_BYTES == 103968);
  static_assert(BoardConfig::DEFAULT_DEVICE.input.power == -1);
  static_assert(BoardConfig::DEFAULT_DEVICE.sdmmc.clk == 38);
  static_assert(BoardConfig::DEFAULT_DEVICE.sdmmc.cmd == 42);
  static_assert(BoardConfig::DEFAULT_DEVICE.sdmmc.d0 == 44);
  assert(BoardConfig::selectDevice(BoardConfig::Board::ReadPico));
  assert(!BoardConfig::selectDevice(BoardConfig::Board::XteinkX4));
  assert(BoardConfig::isReadPico() && !BoardConfig::hasHomeKey());
#else
  static_assert(FREEINK_MCU_C3 && !FREEINK_MCU_S3);
  static_assert(!FREEINK_DRIVER_READPICO && FREEINK_DRIVER_SSD1677);
  static_assert(!FREEINK_FB_PSRAM && !FREEINK_SD_SDMMC);
  static_assert(BoardConfig::MAX_FRAMEBUFFER_BYTES == 48000);
  assert(BoardConfig::selectDevice(BoardConfig::Board::XteinkX4));
  assert(!BoardConfig::selectDevice(BoardConfig::Board::ReadPico));
#endif
  std::puts("Board capabilities and framebuffer limits passed");
}
