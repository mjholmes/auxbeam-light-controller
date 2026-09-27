# AuxBeam Light Controller Library

ESP-IDF component for controlling an RA60 light controller with 6 outputs via 2400 baud UART.

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
