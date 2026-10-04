# Read Pico hardware backend

Select `FREEINK_DEVICE_READPICO=1` and link this library alongside the SDK.
Requires ESP32-S3, 8 MB octal PSRAM, Arduino-ESP32 3.3.11 / ESP-IDF 5.5.5,
and SdFat `USE_BLOCK_DEVICE_INTERFACE=1` for native 1-bit SDMMC.

The LCD bus scans **1216 x 684**, 16 bits wide. FreeInk retains its byte-aligned
1bpp geometry (103,968 bytes); the consumer's portrait view is 684 x 1216.
The backend expands MSB-first 1bpp into epdiy's even-pixel-low-nibble 4bpp.
epdiy retains front/back/difference planes in PSRAM (1,663,488 bytes), plus
internal line/DMA queues. Buffers are allocated once, not on each page turn.
Initial bring-up uses 80 MHz Flash/PSRAM. The BSP requests a 12 MHz pixel clock;
epdiy lowers it to 6 MHz when the prebuilt IDF core uses 32-byte cache lines.

## Ownership

- One IDF I2C master bus on SDA39/SCL40 is owned by the BSP. Do not initialize
  Arduino `Wire` on the same port for this board.
- One mutex serializes PMU commands, polling, touch reset, and display power.
  Normal operation drains the event FIFO through one shared poller. Battery
  reads share its status; the shutdown handshake takes exclusive ownership.
- Refresh completion schedules HV power-off after 8 seconds of idle, matching
  the demo. Adjacent refreshes keep the rails up rather than paying the 500ms
  discharge wait on every page. Sleep/shutdown still power off immediately.
  The idle deadline also applies to `turnOff=false`: the panel remains powered
  just after the draw, while idle power retention is bounded by the board policy.
- Factory VCOM is read from CW32 before any panel refresh. No calibration
  command is issued. Missing calibration leaves the display unpowered.
- CST836U screen coordinates are transformed to native scan coordinates.
  Off-screen keys are classified first: Page Back, Back, Page Forward.
  Cover-key state changes commit immediately; only the PMU power key uses
  mechanical debounce. A release after a slow display update must not be
  reported as a continued hold to the consumer's menu repeat handler.
- SD uses the existing SDK SDMMC block device and SdFat. The demo's FatFS
  mount/format code is deliberately excluded.
- `shutdownRequested()` latches critical battery/PMU requests. The consumer
  must save state and close files, then call `PowerManager`'s sleep path.
  CW32 soft sleep drops ESP EN; waking is a cold boot. GPIO41 is not an RTC
  wake pin. No ESP deep-sleep GPIO wake source is armed for it.

Grayscale overlay and absolute planes are combined into the existing 4bpp
front buffer before one panel activation. No extra full-frame buffer is needed.
Full and Half cleanup requests use GC16; ordinary grayscale uses GL16 with
all pixels driven so unchanged white pixels receive the panel's white push.
Fast uses differential DU. Window
refresh falls back to the full-frame difference calculation. RTC, acceleration
gestures, audio, and USB MSC are not yet enabled. Physical grayscale and refresh
behavior still require device verification.

## Pinned vendor sources

Sources come from [MindReset/read_pico_firmware](https://github.com/MindReset/read_pico_firmware/tree/28cde682a4468a581c278761922724f57d976418),
commit `28cde682a4468a581c278761922724f57d976418`. `vendor-manifest.json` records
every original file's SHA-256. Preserve vendor copyright and license files:
epdiy is LGPL-3.0-or-later; the board/chip/PMU/waveform components are Apache-2.0.

The build excludes epdiy's unused `font.c` (CrossPoint owns font rendering).
The source subset excludes the demo UI, font engine, SD/FatFS, sensors, buzzer,
and IDF 6-only Zbit Flash HPM override. Update deliberately using a clean
checkout and `python scripts/vendor_readpico.py <demo-checkout>`; update the
script's commit pin and review compatibility rather than following a branch.

## Host checks

```sh
c++ -std=c++20 -Wall -Wextra -Werror -Iinclude test/host/test_geometry.cpp -o geometry-test
./geometry-test
```

These checks cover packing/polarity, portrait-to-scan corners, and key-region
boundaries. They do not validate physical display timing or power behavior.
`test/host/test_power.cpp` covers the idle timeout, consecutive refreshes,
cancellation, and the 32-bit timer wrap boundary.
