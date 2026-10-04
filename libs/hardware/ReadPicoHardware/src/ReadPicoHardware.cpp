#include "ReadPicoHardware.h"

#include <Arduino.h>
#include <esp_heap_caps.h>
#include <esp_log.h>
#include <esp_system.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

#include <cstdarg>
#include <cstdio>

#include "ReadPicoPowerPolicy.h"

extern "C" {
#include "../vendor/cst836u/include/cst836u.h"
#include "../vendor/e0470_epaper_waveform/include/e0470_epaper_waveform.h"
#include "../vendor/epdiy/src/epd_highlevel.h"
#include "../vendor/epdiy/src/epdiy.h"
#include "../vendor/read_pico/include/read_pico_board.h"
#include "../vendor/read_pico_pmu/include/read_pico_pmu.h"
}

namespace freeink::readpico {
namespace {
constexpr const char* TAG = "ReadPico";
int usbLog(const char* format, va_list args) {
  char buffer[192];
  const int length = vsnprintf(buffer, sizeof(buffer), format, args);
  if (length <= 0) return 0;
  const size_t count = length < int(sizeof(buffer)) ? size_t(length) : sizeof(buffer) - 1;
  return int(Serial.write(reinterpret_cast<const uint8_t*>(buffer), count));
}
StaticSemaphore_t mutexStorage;
SemaphoreHandle_t mutex() {
  static SemaphoreHandle_t handle = xSemaphoreCreateMutexStatic(&mutexStorage);
  return handle;
}
struct Lock {
  Lock() { xSemaphoreTake(mutex(), portMAX_DELAY); }
  ~Lock() { xSemaphoreGive(mutex()); }
};

bool bootAttempted = false;
bool bootReady = false;
bool displayReady = false;
bool sleepRequested = false;
bool keyWasObserved = false;
unsigned long lastPoll = 0;
unsigned long lastGoodPoll = 0;
bool pollHealthy = false;
unsigned long keyPulseUntil = 0;
cst836u_handle_t touch = nullptr;
EpdiyHighlevelState displayState = {};
bool firstRefresh = true;
bool grayAbsolute = false;
bool grayFull = false;
uint8_t grayPlanes = 0;
uint16_t grayRows[2] = {};
bool grayPending = false;
DisplayPowerPolicy displayPower;

bool criticalEvent(uint8_t type) {
  return type == PMU_EVT_BATTERY_CRITICAL || type == PMU_EVT_BATTERY_SOC_LOW || type == PMU_EVT_SHUTDOWN_REQUESTED ||
         type == PMU_EVT_KEY_FORCE_OFF;
}

// This is the only event FIFO consumer. Battery/UI readers use its cached status.
bool pollLocked(bool force = false) {
  if (!bootReady) return false;
  const unsigned long now = millis();
  if (!force && lastPoll && now - lastPoll < 20) return pollHealthy;
  lastPoll = now;
  pollHealthy = read_pico_pmu_poll() == ESP_OK;
  if (!pollHealthy) return false;
  lastGoodPoll = now;
  const auto* status = read_pico_pmu_get();
  if (status->key_state & 1) keyWasObserved = true;
  if (status->flags & PMU_STATUS_BATTERY_CRITICAL) sleepRequested = true;
  for (int i = 0; i < PMU_EVENT_FIFO_DEPTH && status->event_ok && status->pending_events; ++i) {
    const uint8_t type = status->event.type;
    const uint16_t id = status->event.event_id;
    if (criticalEvent(type)) sleepRequested = true;
    if (type == PMU_EVT_KEY_SHORT) {
      if (!keyWasObserved) keyPulseUntil = now + 80;
      keyWasObserved = false;
    }
    if (!id || read_pico_pmu_event_ack(id) != ESP_OK) break;
    if (read_pico_pmu_poll() != ESP_OK) {
      pollHealthy = false;
      break;
    }
  }
  return pollHealthy && status->status_ok;
}
}  // namespace

bool ensureBooted() {
  Lock lock;
  if (bootAttempted) return bootReady;
  bootAttempted = true;
  // GPIO43 is the touch IRQ. IDF peripheral logs must not use UART0's TX pin.
  esp_log_set_vprintf(usbLog);
  esp_log_level_set(TAG, ESP_LOG_INFO);
  // epdiy allocates four 4bpp-equivalent planes in PSRAM, plus internal DMA queues.
  if (heap_caps_get_free_size(MALLOC_CAP_SPIRAM) < GRAY_BYTES * 4 + BW_BYTES) {
    ESP_LOGE(TAG, "Insufficient PSRAM for E0470 framebuffer and difference planes");
    return false;
  }
  epd_init(&epd_board_read_pico, &E0470_DISPLAY, EPD_LUT_1K);
  epd_set_rotation(EPD_ROT_LANDSCAPE);  // CrossPoint owns orientation.
  const read_pico_pmu_config_t config = READ_PICO_PMU_CONFIG_DEFAULT();
  if (read_pico_pmu_init(read_pico_i2c_bus(), &config) != ESP_OK || read_pico_pmu_report_ready() != ESP_OK) {
    ESP_LOGE(TAG, "PMU initialization / HOST_READY failed");
    return false;
  }
  int vcomMv = 0;
  esp_err_t vcomResult = ESP_ERR_NOT_FOUND;
  for (int attempt = 0; attempt < 3 && vcomResult != ESP_OK; ++attempt) {
    vcomResult = read_pico_pmu_vcom_get(&vcomMv);
  }
  if (vcomResult != ESP_OK) {
    ESP_LOGE(TAG, "No valid factory VCOM in PMU; display remains unpowered");
    return false;
  }
  epd_set_vcom(vcomMv);
  ESP_LOGI(TAG, "PMU ready, calibrated VCOM -%d mV", vcomMv);
  const cst836u_config_t tp = {CST836U_ADDR_DEFAULT, CST836U_I2C_HZ_DEFAULT, GPIO_NUM_43, GPIO_NUM_NC,
                               read_pico_touch_reset};
  if (cst836u_init(read_pico_i2c_bus(), &tp, &touch) != ESP_OK) {
    ESP_LOGE(TAG, "CST836U initialization failed");
  }
  bootReady = true;
  pollLocked(true);
  return true;
}

bool touchReady() {
  if (!ensureBooted()) return false;
  Lock lock;
  return touch != nullptr;
}

bool readTouch(TouchFrame& frame) {
  frame = {};
  if (!ensureBooted()) return false;
  Lock lock;
  if (!touch) return false;
  cst836u_touch_t sample = {};
  if (cst836u_read(touch, &sample) != ESP_OK) return false;
  for (uint8_t i = 0; i < sample.count && i < 2; ++i) {
    if (!sample.points[i].active) continue;
    frame.contacts[frame.count++] = {sample.points[i].id, sample.points[i].x, sample.points[i].y};
  }
  static uint8_t lastCount = 0;
  static TouchRegion lastRegion = TouchRegion::Invalid;
  uint16_t x = 0, y = 0;
  const auto region = frame.count ? mapTouch(frame.contacts[0].x, frame.contacts[0].y, x, y) : TouchRegion::Invalid;
  if (frame.count != lastCount || region != lastRegion) {
    ESP_LOGI(TAG, "Touch count=%u region=%u raw=%u,%u", frame.count, unsigned(region), frame.contacts[0].x,
             frame.contacts[0].y);
    lastCount = frame.count;
    lastRegion = region;
  }
  return true;
}

uint8_t buttons() {
  if (!ensureBooted()) return 0;
  Lock lock;
  if (displayPower.takeIdlePowerOff(millis()) && read_pico_rails_on()) {
    ESP_LOGI(TAG, "Idle display power off");
    epd_poweroff();
  }
  pollLocked();
  if (!lastGoodPoll || millis() - lastGoodPoll > 100) return 0;
  return ((read_pico_pmu_get()->key_state & 1) || millis() < keyPulseUntil) ? (1 << 6) : 0;
}

bool shutdownRequested() {
  Lock lock;
  return sleepRequested;
}

bool readBattery(BatteryStatus& status) {
  status = {};
  if (!ensureBooted()) return false;
  Lock lock;
  if (!pollLocked()) return false;
  const auto* sample = read_pico_pmu_get();
  status.valid = (sample->flags & PMU_STATUS_BATTERY_VALID) && sample->soc_permille <= 1000;
  status.millivolts = sample->battery_mv;
  status.percentage = sample->soc_permille / 10;
  status.chargingKnown = sample->charge_state != PMU_CHARGE_UNKNOWN;
  status.charging = sample->charge_state == PMU_CHARGE_CHARGING;
  return status.valid;
}

bool beginDisplay() {
  if (!ensureBooted()) return false;
  Lock lock;
  if (displayReady) return true;
  e0470_waveform_init();
  displayState = epd_hl_init(&E0470_WAVEFORM);
  // One persistent set of PSRAM buffers: no allocation on page turns.
  epd_hl_set_all_white(&displayState);
  displayReady = true;
  ESP_LOGI(TAG, "Display 1216x684, 16-bit LCD, PSRAM free %u, internal free %u",
           unsigned(heap_caps_get_free_size(MALLOC_CAP_SPIRAM)),
           unsigned(heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT)));
  return true;
}

static void refreshLocked(bool full, bool fast) {
  displayPower.cancel();
  ESP_LOGI(TAG, "Refresh %s", full || firstRefresh ? "GC16 full" : fast ? "DU partial" : "GL16 full pixels");
  epd_poweron();
  if (!read_pico_rails_on()) {
    ESP_LOGE(TAG, "EPD rails failed to start");
    epd_poweroff();
    return;
  }
  const int temperature = int(epd_ambient_temperature());
  if (firstRefresh) epd_clear();
  // E0470 GL16's white push must also drive unchanged white pixels.
  const auto result = full || firstRefresh ? epd_hl_update_screen_full(&displayState, MODE_GC16, temperature)
                      : fast               ? epd_hl_update_screen(&displayState, MODE_DU, temperature)
                                           : epd_hl_update_screen_full(&displayState, MODE_GL16, temperature);
  if (result != EPD_DRAW_SUCCESS) {
    ESP_LOGE(TAG, "Display update error 0x%x", unsigned(result));
  } else {
    firstRefresh = false;
  }
  // Keep rails up immediately after a draw, including turnOff=false callers,
  // but never retain the panel's HV indefinitely once the reader is idle.
  displayPower.refreshed(millis());
}

void displayBw(const uint8_t* framebuffer, bool full, bool fast, bool /*turnOff*/) {
  if (!framebuffer || !beginDisplay()) return;
  Lock lock;
  grayPlanes = 0;
  grayPending = false;
  expandBw(framebuffer, displayState.front_fb, BW_BYTES);
  refreshLocked(full, fast);
}

void beginGray(const uint8_t* framebuffer, bool absolute, bool full) {
  if (!framebuffer || !beginDisplay()) return;
  Lock lock;
  grayPlanes = 0;
  grayRows[0] = grayRows[1] = 0;
  grayPending = true;
  grayAbsolute = absolute;
  grayFull = full;
  expandBw(framebuffer, displayState.front_fb, BW_BYTES);
}

void uploadGrayPlane(const uint8_t* plane, bool msb) { uploadGrayRows(plane, msb, 0, HEIGHT); }

void uploadGrayRows(const uint8_t* rows, bool msb, uint16_t yStart, uint16_t numRows) {
  if (!rows || !displayReady || !numRows || yStart >= HEIGHT || numRows > HEIGHT - yStart) return;
  Lock lock;
  const unsigned plane = msb ? 1 : 0;
  // Require complete, contiguous LSB then MSB uploads before activation.
  if (!grayPending || yStart != grayRows[plane] || (msb && grayPlanes != 1)) return;
  applyGrayPlane(rows, displayState.front_fb + size_t(yStart) * WIDTH / 2, size_t(numRows) * WIDTH / 8, msb,
                 grayAbsolute);
  grayRows[plane] += numRows;
  if (grayRows[plane] == HEIGHT) grayPlanes |= msb ? 2 : 1;
}

void displayGray(bool /*turnOff*/) {
  Lock lock;
  if (grayPlanes != 3) return;
  grayPlanes = 0;
  grayPending = false;
  refreshLocked(grayFull, false);
}

void cleanupGray(const uint8_t* bw) {
  Lock lock;
  if (!grayPending || !bw) return;
  // The host could not finish composing grays (e.g. scratch OOM). Commit its
  // intact B/W frame once instead of leaving a deferred page undisplayed.
  expandBw(bw, displayState.front_fb, BW_BYTES);
  grayPending = false;
  grayPlanes = 0;
  refreshLocked(grayFull, false);
}

void sleepDisplay() {
  Lock lock;
  displayPower.cancel();
  if (bootReady) epd_poweroff();
}

[[noreturn]] void softSleep() {
  if (ensureBooted()) {
    Lock lock;
    displayPower.cancel();
    epd_poweroff();
    if (touch) cst836u_set_mode(touch, CST836U_MODE_DEEPSLEEP);
    // GPIO41 is not RTC-capable. CW32 drops ESP EN and restarts it on the key.
    const esp_err_t err = sleepRequested ? read_pico_pmu_power_off() : read_pico_pmu_report_sleep();
    if (err != ESP_OK) {
      ESP_LOGE(TAG, "PMU sleep / shutdown failed: %s; restarting for recovery", esp_err_to_name(err));
      esp_restart();
    }
  } else {
    ESP_LOGE(TAG, "PMU unavailable; cannot arm sleep, restarting for recovery");
    esp_restart();
  }
  // A successful PMU handshake normally holds EN low before this is reached.
  for (;;) delay(1000);
}
}  // namespace freeink::readpico
