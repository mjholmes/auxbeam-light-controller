# Architecture

## Layers

```
auxbeam_light.c        Public API: init/deinit, set_output, set_multiple,
                        process, set_timeout, get_pending_count. Owns the
                        single static auxbeam_context_t and orchestrates
                        the layers below.
    |
    +-- command_queue.c     Pending-command tracking: a fixed 4-slot table,
    |                       sequence ID assignment/wraparound, timeout
    |                       detection. A single static global (see
    |                       "Single-instance design" below) -- not a
    |                       member of auxbeam_context_t despite the two
    |                       having originally been designed together.
    |
    +-- light_protocol.c    Wire-format encode/decode. Zero ESP-IDF
    |                       dependencies -- pure C, portable to any target
    |                       or host.
    |
    +-- checksum.c          The 8-bit sum-mod-256 checksum. Also portable.
    |
    +-- hal_uart_esp32.c    The only ESP-IDF-specific file: wraps
                            driver/uart.h behind hal_uart.h's
                            platform-independent interface. Swappable for
                            test/host/mock_hal_uart.c on a development
                            machine.
```

Protocol field layout and semantics are documented separately in
[PROTOCOL.md](PROTOCOL.md).

## Single-instance design

There is exactly one `auxbeam_context_t` and one command queue, both static
globals, not heap-allocated. `auxbeam_light_init()` fails with
`AUXBEAM_ERR_ALREADY_INITIALIZED` if called again before a matching
`auxbeam_light_deinit()`. This is a deliberate choice, not a limitation
nobody got around to fixing: the library targets one RA60 controller on one
UART, and static allocation keeps the memory footprint fixed and
measurable (see "Resource usage" below) rather than subject to heap
fragmentation on a long-running embedded system.

## Concurrency model: single owner task

**This library has no internal locking.** Every function that touches
shared state does so directly against the static globals above, with no
mutex, critical section, or atomic operation anywhere.

That's safe only if exactly one FreeRTOS task ever calls into the library.
`auxbeam_light_init()` records the calling task's handle
(`xTaskGetCurrentTaskHandle()`); every other function -- `set_output`,
`set_multiple`, `process`, `set_timeout`, `get_pending_count`, `deinit` --
checks the current task against that recorded owner and returns
`AUXBEAM_ERR_WRONG_TASK` if they don't match, rather than racing on the
queue or context.

The expected integration pattern: one task owns this library end to end --
it calls `init()`, sends commands, and calls `process()` periodically, all
from its own loop. If other parts of the application (a UI task, a CAN bus
listener, etc.) need to control lights, they send a request to that owning
task (a FreeRTOS queue works well) rather than calling
`auxbeam_light_*()` directly. None of these functions are ISR-safe either.

This matters specifically on ESP32's dual-core SMP FreeRTOS: without the
guard, two tasks pinned to different cores calling `set_output()`
concurrently could both claim the same "first free" queue slot, silently
dropping one command -- a real, hard-to-reproduce bug, not a theoretical
one. The guard trades a small amount of flexibility (no true multi-task
producer access) for catching that class of bug immediately and loudly
instead of letting it corrupt state.

## Frame reassembly

At 2400 baud an 11-byte frame takes ~46ms to arrive (see
[PROTOCOL.md](PROTOCOL.md#timing)), so a single non-blocking
`hal_uart_receive()` call inside `auxbeam_light_process()` will often see
only part of a frame. The context carries a small `rx_buf`/`rx_len` pair
that accumulates bytes across calls to `process()`; only once a full 11
bytes have arrived does the library attempt to decode and checksum-validate
the frame.

If that decode fails -- wrong message/command type, or a bad checksum --
the window may simply be misaligned (a stray or dropped byte shifted
everything by one), not necessarily garbage. Rather than discard the whole
11 bytes and stay at the same wrong offset forever, `process()` drops only
the oldest byte and retries at the next alignment on the following call.
Since every possible byte offset gets tried this way, a one-byte desync
self-heals within at most ~10 extra polls (about 100ms at the documented
10ms interval) instead of permanently breaking decoding -- see
`test_resync_after_stray_byte()` in `test_integration.c` for the regression
test (this was a real, externally-reported bug: a single stray byte broke
the link permanently until reinit).

## Error handling

- Every command is checksum-validated on decode; a bad checksum is logged
  and the frame is discarded (the command stays pending until a valid ACK
  or the timeout fires).
- Acknowledgments are matched to pending commands by sequence ID, so
  out-of-order ACKs (a real scenario with up to 4 commands in flight at
  once) resolve correctly regardless of arrival order.
- `auxbeam_light_deinit()` cancels any still-pending commands without
  invoking their callbacks -- an explicit, documented behavior, not a
  side effect.
- Both callback sites (`ack_cb` on a matched ACK, `timeout_cb` on
  expiry) free the command's queue slot *before* invoking the callback,
  specifically so that a retry from inside `timeout_cb` -- the pattern
  `auxbeam_light.h` documents as safe, and `examples/quickstart.c`
  demonstrates -- sees an accurate queue count rather than one still
  counting its own about-to-be-freed slot. Getting this ordering backwards
  was a real, externally-reported bug: with 4 commands in flight, retrying
  from the timing-out one's own callback would incorrectly fail with
  `AUXBEAM_ERR_QUEUE_FULL`. See `test_retry_from_timeout_callback_when_queue_full()`
  in `test_integration.c`.
- Debug builds assert that the queue's `pending_count` always matches its
  actual active-slot count, catching queue-management bugs immediately
  rather than letting `pending_count` silently drift.

## Resource usage

Static RAM footprint (context + queue + UART handle), measured via a
host-side struct-size check with 32-bit pointers (approximating the ESP32's
word size, since no ESP-IDF toolchain was available to produce a real
linker map at the time this was measured): **244 bytes**, well under
budget for a resource-constrained target. This has not yet been confirmed
with a real `idf.py` build's linker map, nor has flash usage been measured
the same way -- both are open items (see "Known limitations" below).

## Testing strategy

`test/host/` builds the real production source files -- not reimplemented
test doubles -- against a software mock of `hal_uart.h`
(`mock_hal_uart.c`) and minimal FreeRTOS/ESP-IDF stub headers, so the whole
suite runs on a development machine with plain `gcc`/`cmake`, no ESP32 or
RA60 hardware required:

- `test_checksum.c` -- checksum edge cases, including the 256-byte-sum wrap.
- `test_protocol.c` -- encode/decode correctness and ACK rejection (bad
  checksum, wrong message/command type).
- `test_queue.c` -- allocate/find/free/cancel, the queue-full boundary,
  sequence ID wraparound at 256, and the timeout path.
- `test_integration.c` -- end-to-end via the public API: the frame-
  reassembly path (a real regression test for the bug described above),
  timeout handling, out-of-order ACK matching, `set_multiple()`,
  queue-full via the real API, `deinit()`'s cancel-without-callback
  behavior, and the owner-task guard.

Uses a small custom assertion harness (`test_util.h`) rather than a
Unity+CMock setup, as a deliberate dependency tradeoff for a project this
size.

## Known limitations

- **No real target build has been verified.** Every fix in this codebase
  was validated on the host test suite; `idf.py build` against an actual
  ESP-IDF install (for real flash-size figures and target-specific
  compilation) hasn't been run.
- `test/target/` (hardware-in-loop tests against real ESP32 + RA60
  hardware) is not implemented.
- Single controller instance only -- see "Single-instance design" above.
