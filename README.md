# auxbeam-light-controller

[![Host tests](https://github.com/mjholmes/auxbeam-light-controller/actions/workflows/host-tests.yml/badge.svg)](https://github.com/mjholmes/auxbeam-light-controller/actions/workflows/host-tests.yml)

ESP-IDF library for controlling an AuxBeam RA60 light controller -- 6
outputs, each independently OFF/ON/STROBE -- over a 2400 baud UART link,
with acknowledgment matching and timeout detection built in.

**Status**: pre-1.0, host-tested (593 checks, no ESP32/RA60 hardware
required), not yet verified against a real `idf.py` build. See
[docs/ARCHITECTURE.md](docs/ARCHITECTURE.md#known-limitations).

> **Not thread-safe by design.** There is no internal locking. Every API
> call -- including `auxbeam_light_process()` -- must be made from the
> single task that called `auxbeam_light_init()`. If other parts of your
> app (a UI task, a CAN bus listener, etc.) need to control lights, have
> them send requests to that one owning task (a FreeRTOS queue works well)
> instead of calling this library directly. A call from any other task
> returns `AUXBEAM_ERR_WRONG_TASK`. None of these functions are ISR-safe.
> See [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md#concurrency-model-single-owner-task)
> for why.

## Features

- Control 6 light outputs (OFF / ON / STROBE)
- Asynchronous, callback-based command completion (ack / timeout)
- Automatic protocol encoding, checksum validation, and out-of-order ACK matching
- Configurable per-command-queue timeout (4 commands in flight at once)
- Hardware-abstracted: the real production code builds and runs on a
  development machine against a software UART mock -- no ESP32 or RA60
  controller required to test it

## Installation

### Via the ESP-IDF Component Manager (recommended)

Add this to your project's own `idf_component.yml` (or your main
component's, if you don't have a top-level one yet):

```yaml
dependencies:
  auxbeam_light:
    git: "https://github.com/mjholmes/auxbeam-light-controller.git"
    version: "v0.1.0"   # pin to a tag once one exists
```

Then run `idf.py reconfigure` (or just build) -- the Component Manager
fetches it into `managed_components/` automatically.

### Manual copy

Copy this repository's contents into `<your-project>/components/auxbeam_light/`.

## Usage

```c
#include "auxbeam_light.h"

void on_ack(uint8_t seq_id, void *user_data) {
    ESP_LOGI("app", "Command %d acknowledged", seq_id);
}

void on_timeout(uint8_t seq_id, auxbeam_error_t error, void *user_data) {
    ESP_LOGW("app", "Command %d failed: %d", seq_id, error);
}

// Call init() from the task that will own the library -- every later call,
// including process(), must come from this same task.
void light_controller_task(void *param) {
    auxbeam_config_t config = {
        .uart_num = UART_NUM_1,
        .tx_pin = 17,
        .rx_pin = 16,
        .default_timeout_ms = 500,
    };
    auxbeam_context_t *ctx;
    if (auxbeam_light_init(&config, &ctx) != AUXBEAM_OK) {
        return;
    }

    auxbeam_light_set_output(ctx, 0, AUXBEAM_OUTPUT_ON, on_ack, on_timeout, NULL);

    while (1) {
        // ... send more commands here, or drain a queue of requests from
        // other tasks ...
        auxbeam_light_process(ctx);   // check for ACKs/timeouts
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}
```

See [examples/](examples/) for fuller, runnable-shaped examples covering
retry-on-timeout and queue-full backoff patterns.

## Documentation

- [docs/PROTOCOL.md](docs/PROTOCOL.md) -- the RA60 wire protocol
- [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) -- library internals, the
  concurrency model, and known limitations
- [include/auxbeam_light.h](include/auxbeam_light.h) -- the complete public API
- [CHANGELOG.md](CHANGELOG.md)

## Testing

```bash
cd test/host
mkdir build && cd build
cmake .. && make
./test_runner
```

Runs the full suite (checksum, protocol encode/decode, command queue, and
end-to-end integration) against a software UART mock. No ESP-IDF toolchain
needed.

## Hardware

- ESP32 (ESP-WROOM-32, ESP32-S2/S3/C3) with a spare UART
- AuxBeam RA60 (or compatible) light controller
- Wiring: ESP32 TX -> controller RX, ESP32 RX -> controller TX, common ground
- UART: 2400 baud, 8N1, no flow control

## License

Apache-2.0 -- see [LICENSE](LICENSE).
