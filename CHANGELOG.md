# Changelog

Format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/);
versioning follows [Semantic Versioning](https://semver.org/) once tagged
releases begin.

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
