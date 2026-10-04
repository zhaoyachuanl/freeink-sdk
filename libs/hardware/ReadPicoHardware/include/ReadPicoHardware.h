#pragma once

#include <cstdint>

#include "ReadPicoGeometry.h"

namespace freeink::readpico {
struct BatteryStatus {
  bool valid = false;
  uint16_t millivolts = 0;
  uint16_t percentage = 0;
  bool chargingKnown = false;
  bool charging = false;
};
struct TouchFrame {
  uint8_t count = 0;
  struct Contact {
    uint8_t id;
    uint16_t x;
    uint16_t y;
  } contacts[2] = {};
};

bool ensureBooted();
bool touchReady();
bool readTouch(TouchFrame& frame);
uint8_t buttons();
bool shutdownRequested();
bool readBattery(BatteryStatus& status);
bool beginDisplay();
void displayBw(const uint8_t* framebuffer, bool full, bool fast, bool turnOff);
void beginGray(const uint8_t* framebuffer, bool absolute, bool full);
void uploadGrayPlane(const uint8_t* plane, bool msb);
void uploadGrayRows(const uint8_t* rows, bool msb, uint16_t yStart, uint16_t numRows);
void displayGray(bool turnOff);
void cleanupGray(const uint8_t* bw);
void sleepDisplay();
// The consumer must persist state and close SD handles before calling this.
[[noreturn]] void softSleep();
}  // namespace freeink::readpico
