#include <ReadPicoPowerPolicy.h>

#include <cassert>
#include <cstdio>
#include <limits>

using freeink::readpico::DisplayPowerPolicy;

int main() {
  DisplayPowerPolicy policy;
  assert(!policy.takeIdlePowerOff(100000));
  policy.refreshed(1000);
  assert(!policy.takeIdlePowerOff(8999));
  policy.refreshed(8000);
  assert(!policy.takeIdlePowerOff(15999));
  assert(policy.takeIdlePowerOff(16000));
  assert(!policy.takeIdlePowerOff(17000));
  policy.refreshed(20000);
  policy.refreshed(21000);
  assert(!policy.takeIdlePowerOff(28999));
  assert(policy.takeIdlePowerOff(29000));
  policy.refreshed(50000);
  policy.cancel();
  assert(!policy.takeIdlePowerOff(60000));
  const uint32_t start = std::numeric_limits<uint32_t>::max() - 100;
  policy.refreshed(start);
  assert(!policy.takeIdlePowerOff(uint32_t(start + 7999)));
  assert(policy.takeIdlePowerOff(uint32_t(start + 8000)));
  std::puts("Display power idle timeout, burst keepalive, cancellation and timer wrap passed");
}
