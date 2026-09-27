# Hardware-in-loop tests

Not implemented yet. This directory is a placeholder for tests that run on
real ESP32 hardware against a real RA60 controller -- validating actual
UART timing, real acknowledgment latency, and behavior when the controller
is disconnected or powered off, none of which the host mock in
[`test/host/`](../host/) can exercise.

Until these exist, treat every fix and feature here as verified on the
host test suite only, not on real hardware. See
[docs/ARCHITECTURE.md](../../docs/ARCHITECTURE.md#known-limitations).
