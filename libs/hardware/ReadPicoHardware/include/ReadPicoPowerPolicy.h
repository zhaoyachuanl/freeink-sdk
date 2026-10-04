#pragma once

#include <cstdint>

namespace freeink::readpico {
class DisplayPowerPolicy {
  uint32_t lastRefresh = 0;
  bool idlePending = false;

 public:
  static constexpr uint32_t IDLE_MS = 8000;
  void cancel() { idlePending = false; }
  void refreshed(uint32_t now) {
    lastRefresh = now;
    idlePending = true;
  }
  bool takeIdlePowerOff(uint32_t now) {
    if (!idlePending || uint32_t(now - lastRefresh) < IDLE_MS) return false;
    idlePending = false;
    return true;
  }
};
}  // namespace freeink::readpico
