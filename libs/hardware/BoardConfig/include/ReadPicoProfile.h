#pragma once

// Included inside BoardConfig after its shared profile defaults are declared.
constexpr BoardProfile READ_PICO = {
    Board::ReadPico,
    "readpico",
    InputStyle::DigitalButtons,
    DisplayController::ReadPico,
    1216,
    684,
    {PIN_UNASSIGNED, PIN_UNASSIGNED, PIN_UNASSIGNED, PIN_UNASSIGNED, PIN_UNASSIGNED, PIN_UNASSIGNED, PIN_UNASSIGNED},
    0,
    {PIN_UNASSIGNED, PIN_UNASSIGNED, PIN_UNASSIGNED, PIN_UNASSIGNED, PIN_UNASSIGNED, false, 0},
    {PIN_UNASSIGNED, PIN_UNASSIGNED, PIN_UNASSIGNED, PIN_UNASSIGNED, PIN_UNASSIGNED, PIN_UNASSIGNED, PIN_UNASSIGNED,
     false},
    PIN_UNASSIGNED,
    PIN_UNASSIGNED,
    1.0f,
    PIN_UNASSIGNED,
    {TouchController::Cst836u, 39, 40, 43, PIN_UNASSIGNED, 0x15, 0, 1215, 0, 683, false, 0, true, false, PIN_UNASSIGNED,
     true, false, true, false},
    NO_FRONTLIGHT,
    NO_AUDIO,
    NO_LEDS,
    NO_FLIP,
    {38, 42, 44, PIN_UNASSIGNED, PIN_UNASSIGNED, PIN_UNASSIGNED, 1},
    {39, 40, 400000, 0x2A, 0, 0, GaugeType::ReadPico},
    NO_MIC,
    NO_SENSORS,
    1.5f,
};
static_assert(READ_PICO.displayWidth % 8 == 0);
static_assert(READ_PICO.displayWidth / 8 * READ_PICO.displayHeight == 103968);
