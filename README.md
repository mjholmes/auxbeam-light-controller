# AuxBeam Light Controller Library

ESP-IDF component for controlling an RA60 light controller with 6 outputs via 2400 baud UART.

**Not thread-safe by design.** There is no internal locking. Every API call
-- including `auxbeam_light_process()` -- must be made from the single task
that called `auxbeam_light_init()`. If other parts of your app (UI, CAN bus
listener, etc.) need to control lights, have them send requests to that one
owning task (a FreeRTOS queue works well) instead of calling this library
directly. A call from any other task returns `AUXBEAM_ERR_WRONG_TASK`. None
of these functions are ISR-safe.

## Features

- Control 6 light outputs (OFF/ON/STROBE modes)
- Asynchronous operation with callbacks
- Automatic protocol encoding and checksum validation
- Timeout detection with configurable thresholds
- Hardware-abstracted design for testability

## Installation

Copy this component directory into your ESP-IDF project's `components/` folder.

## Usage

See `/specs/001-light-controller/quickstart.md` for complete usage examples.

## API Reference

See `include/auxbeam_light.h` for the complete public API.

## License

TBD
