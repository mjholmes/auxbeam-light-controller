# Examples

These are illustrative source files, not standalone buildable ESP-IDF
projects -- copy the relevant parts into your own `main.c` rather than
trying to build this directory directly. (Turning them into full example
projects with their own `main/CMakeLists.txt` is a reasonable future
improvement, but wasn't done here since there's no ESP-IDF toolchain
available to verify one actually builds -- see
[docs/ARCHITECTURE.md](../docs/ARCHITECTURE.md#known-limitations).)

- **basic_example.c** -- the minimal init -> send -> process -> loop
  lifecycle, cycling through ON / STROBE / OFF patterns.
- **quickstart.c** -- the same lifecycle plus two patterns worth copying
  directly: retrying a command a bounded number of times on timeout, and
  backing off (while still calling `process()`) when the 4-command queue
  is full instead of just returning `AUXBEAM_ERR_QUEUE_FULL` to the caller.

Both run everything -- `init()`, sending commands, and `process()` -- from
a single task, per the concurrency model described in
[docs/ARCHITECTURE.md](../docs/ARCHITECTURE.md#concurrency-model-single-owner-task).
