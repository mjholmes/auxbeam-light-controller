# Changelog

Format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/);
versioning follows [Semantic Versioning](https://semver.org/) once tagged
releases begin.

## [Unreleased]

### Fixed
- **L-6**: output nibbles were packed in the wrong order. The RA60 expects
  the lower-numbered output in the HIGH nibble of each byte (panel capture:
  CH1 press `38 88 88`, CH2 press `83 88 88`); the encoder put it in the low
  nibble, so commands for outputs 2/3 drove outputs 1/4 instead.
  `light_protocol_encode_command()` fixed; test vectors from panel captures.
- **L-5**: acknowledgments from a real RA60 were all rejected. The decoder
  required byte 2 (`cmd_type`) to be exactly `0xF8`, but the RA60 replies
  with `0x18` (captured on hardware: `00 FF 18 00 08 80 88 00 00 00 27`), so
  every command timed out. `light_protocol_decode_ack()` now checks only the
  low nibble (`0x8`); byte 1 must still be `0xFF` and the checksum valid.
- **L-1**: a single stray or dropped byte on the wire would permanently
  desynchronize `process()`'s frame decoding -- reported and reproduced by
  an external review (5 valid ACKs sent after one stray byte matched 0 of
  5). `process()` now byte-slips one byte at a time and retries when a
  full 11-byte window fails to decode, instead of discarding the whole
  window and staying at the same wrong offset forever. Self-heals within
  ~10 extra polls of any single-byte desync.
- **L-2**: retrying a command from inside its own `timeout_cb` -- the
  pattern `auxbeam_light.h` documents as safe and `examples/quickstart.c`
  demonstrates -- could fail with `AUXBEAM_ERR_QUEUE_FULL` when 4 commands
  were already in flight, because both callback sites (`ack_cb` and
  `timeout_cb`) freed their queue slot *after* invoking the callback. Now
  freed before, so a reentrant retry sees an accurate queue count.

## [0.1.0] - 2026-09-27

Initial standalone extraction from the `auxbeam-serial` application repo's
`components/auxbeam_light/`, with full commit history preserved.

### Added
- Public API: `auxbeam_light_init`/`deinit`, `set_output`, `set_multiple`,
  `process`, `set_timeout`, `get_pending_count`.
- RA60 11-byte protocol encode/decode with checksum validation.
- Fixed 4-slot pending-command queue with sequence ID matching and
  wraparound, and timeout detection.
- Host-side test suite (593 checks) covering checksum, protocol, queue,
  and end-to-end integration against a software UART mock -- no ESP32/RA60
  hardware required.
- Single-owner-task concurrency guard: every API call after `init()` is
  rejected with `AUXBEAM_ERR_WRONG_TASK` unless made from the task that
  called `init()`, since the library has no internal locking.

### Fixed (since the code was first written)
- A frame-reassembly bug where `process()` assumed one non-blocking UART
  read returned a complete 11-byte frame; at 2400 baud with a 10ms poll
  interval, this would routinely desynchronize decoding. Now accumulates
  bytes across calls until a full frame arrives.
- `deinit()` not cancelling pending commands, contradicting its own
  documented contract.
- A compile-blocking incomplete-type error (`pending_command_t` was only
  forward-declared where it needed to be a complete type) -- the component
  had never actually been built before this was caught.
- Heap allocation (`malloc`/`free`) for the library context and UART
  handle, replaced with static single instances.

### Known limitations
- Not yet verified against a real ESP-IDF target build (`idf.py build`) --
  see [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md#known-limitations).
- `test/target/` (hardware-in-loop tests) not implemented.
- Single controller instance only.
