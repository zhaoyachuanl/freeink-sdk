#include <Arduino.h>
#include <InputManager.h>
#include <ReadPicoHardware.h>

#include <cassert>
#include <cstdio>

namespace {
freeink::readpico::TouchFrame nextFrame;
bool readFails = false;
bool wantShutdown = false;
uint8_t pmuButtons = 0;
void tick(InputManager& input) {
  fakeNow += 25;
  input.update();
}
void point(InputManager& input, uint16_t x, uint16_t y) {
  nextFrame = {1, {{0, x, y}, {}}};
  tick(input);
}
void idle(InputManager& input) {
  nextFrame = {};
  tick(input);
}
}  // namespace

namespace freeink::readpico {
bool ensureBooted() { return true; }
bool touchReady() { return true; }
bool readTouch(TouchFrame& frame) {
  frame = nextFrame;
  return !readFails;
}
uint8_t buttons() { return pmuButtons; }
bool shutdownRequested() { return wantShutdown; }
}  // namespace freeink::readpico

int main() {
  InputManager input;
  input.begin();
  assert(input.hasTouch() && input.supportsMultiTouch());
  float nx = 0, ny = 0;
  point(input, 0, 0);
  assert(input.isTouchPressed());
  assert(input.getTouchPoint().x == 0 && input.getTouchPoint().y == 683);
  idle(input);
  assert(input.wasTouchTap(nx, ny) && nx == 0 && ny == 1);
  idle(input);

  point(input, 240, 1500);
  assert(input.wasPressed(InputManager::BTN_BACK) && !input.isTouchPressed());
  assert(input.capacitivePageButtonMask() == 0);
  assert(!input.wasHomeKeyPressed());
  idle(input);
  assert(input.wasReleased(InputManager::BTN_BACK) && !input.wasTouchTap(nx, ny));
  point(input, 400, 1500);
  assert(input.isPressed(InputManager::BTN_DOWN));
  fakeNow += 1000;
  point(input, 400, 1500);
  // A direct key switch must start a new hold, even without an idle sample.
  point(input, 80, 1500);
  assert(input.wasPressed(InputManager::BTN_UP) && input.wasReleased(InputManager::BTN_DOWN));
  assert(input.getHeldTime() == 0);
  assert(input.isPressed(InputManager::BTN_UP));
  // The first sample after a slow display refresh must commit the released key.
  fakeNow += 700;
  idle(input);
  assert(!input.isPressed(InputManager::BTN_UP) && input.wasReleased(InputManager::BTN_UP));
  idle(input);
  assert(!input.wasReleased(InputManager::BTN_UP) && !input.wasPressed(InputManager::BTN_UP));
  point(input, 400, 1500);
  assert(input.wasPressed(InputManager::BTN_DOWN));
  fakeNow += 700;
  idle(input);
  assert(!input.isPressed(InputManager::BTN_DOWN) && input.wasReleased(InputManager::BTN_DOWN));
  point(input, 80, 1500);
  assert(input.wasPressed(InputManager::BTN_UP));
  fakeNow += 700;
  point(input, 80, 1500);
  assert(input.isPressed(InputManager::BTN_UP) && input.getHeldTime() >= 700);
  assert(!input.wasPressed(InputManager::BTN_UP));
  readFails = true;
  fakeNow += 150;
  tick(input);
  tick(input);
  assert(!input.isPressed(InputManager::BTN_UP));
  readFails = false;
  idle(input);

  point(input, 120, 200);
  readFails = true;
  fakeNow += 150;
  tick(input);
  assert(!input.isTouchPressed() && !input.wasTouchTap(nx, ny));
  readFails = false;
  idle(input);

  nextFrame = {2, {{0, 100, 200}, {1, 300, 200}}};
  tick(input);
  assert(input.getTouchSnapshot().count == 2);
  idle(input);
  assert(!input.wasTouchTap(nx, ny));
  idle(input);
  // A cover key plus a screen contact cannot become a clamped screen tap.
  nextFrame = {2, {{0, 80, 1500}, {1, 300, 200}}};
  tick(input);
  assert(!input.isTouchPressed() && !input.wasHomeKeyPressed());
  idle(input);
  assert(!input.wasTouchTap(nx, ny));

  pmuButtons = 1 << InputManager::BTN_POWER;
  tick(input);
  assert(!input.isPressed(InputManager::BTN_POWER));
  point(input, 400, 1500);
  assert(input.wasPressed(InputManager::BTN_POWER) && input.wasPressed(InputManager::BTN_DOWN));
  tick(input);
  assert(input.isPressed(InputManager::BTN_POWER) && input.isPowerButtonPhysicallyPressed());
  pmuButtons = 0;
  point(input, 80, 1500);
  assert(input.wasPressed(InputManager::BTN_UP) && input.isPressed(InputManager::BTN_POWER));
  tick(input);
  assert(input.wasReleased(InputManager::BTN_POWER));
  wantShutdown = true;
  assert(input.shutdownRequested());
  std::puts("CST836U contacts, cover keys, failed-read cancellation and PMU input passed");
}
