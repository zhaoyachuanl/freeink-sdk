from pathlib import Path

Import("env", "pio_lib_builder")
root = Path(pio_lib_builder.path)
paths = [
    root / "include",
    root / "vendor/epdiy/src",
    root / "vendor/epdiy/include",
    root / "vendor/e0470_epaper_waveform/waveforms",
]
for component in ("cst836u", "fca9555", "sy7636a", "read_pico_pmu", "read_pico", "e0470_epaper_waveform"):
    paths.append(root / "vendor" / component / "include")
env.Append(
    CPPPATH=[str(path) for path in paths],
    CPPDEFINES=[
        ("USE_ESP_IDF_LOG", 1),
        ("LOG_LOCAL_LEVEL", 3),
        ("CONFIG_READ_PICO_I2C_PORT", 0),
        ("CONFIG_READ_PICO_I2C_SDA_GPIO", 39),
        ("CONFIG_READ_PICO_I2C_SCL_GPIO", 40),
        ("CONFIG_READ_PICO_I2C_FREQ_HZ", 400000),
        ("CONFIG_READ_PICO_IOE_INT_GPIO", 41),
        ("CONFIG_READ_PICO_TP_INT_GPIO", 43),
    ],
)
