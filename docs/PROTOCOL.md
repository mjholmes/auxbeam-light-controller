# RA60 Serial Protocol

Reverse-engineered wire protocol for the AuxBeam RA60 light controller, spoken
over a 2400 baud UART link (8 data bits, no parity, 1 stop bit, no flow
control). It's a simple fixed-length, command/acknowledgment protocol shared
with other AuxBeam hardware that supports up to 8 outputs, even though the
RA60 only exposes 6.

## Frame layout

Every frame -- command or acknowledgment -- is 11 bytes:

| Offset | Size | Field | Command value | Acknowledgment value |
|-------:|-----:|-------|----------------|-----------------------|
| 0 | 1 | Sequence ID | echoed back unchanged by the ACK | |
| 1 | 1 | Message type | `0x00` | `0xFF` |
| 2 | 1 | Command type | `0x08` (output-altering command) | `0xF8` or `0x18` (see below) |
| 3 | 1 | Unknown | `0x0C` for output commands (`0x00`/`0x01` seen for master on/off, unverified) | `0x00` |
| 4-7 | 4 | Output state | 8 packed nibbles, one per output (only 6 used) | mirrors the command, not meaningfully validated |
| 8-9 | 2 | Unknown | always `0x00` observed | always `0x00` observed |
| 10 | 1 | Checksum | 8-bit sum of bytes 0-9, mod 256 | same |

The sequence ID doesn't need to increment and isn't checked by the
controller -- it's simply echoed back in the acknowledgment so the sender
knows which command is being acknowledged. An invalid checksum causes the
command to be silently ignored; no response is sent.

### Acknowledgment command type

The low nibble of byte 2 is `0x8` in both directions. In acknowledgments the
high nibble varies: `0xF8` was documented originally, and a real RA60 has been
captured replying `0x18`:

```
command: 00 00 08 0C 08 80 88 00 00 00 24
ACK:     00 FF 18 00 08 80 88 00 00 00 27
```

The meaning of the high nibble is unknown, so the decoder checks only the low
nibble (plus byte 1 = `0xFF`, the checksum, and a matching sequence ID).

### Checksum

```python
def calculate_checksum(data: bytes) -> int:
    return sum(data) % 256
```
Computed over the first 10 bytes; stored in byte 10.

### Output state nibbles

Bytes 4-7 pack 8 nibbles (2 outputs per byte), **lower-numbered output in the
high nibble**: `outputs[0] = (state[0] << 4) | state[1]`,
`outputs[1] = (state[2] << 4) | state[3]`, `outputs[2] = (state[4] << 4) | state[5]`,
`outputs[3]` = outputs 7/8, not present on the RA60 (`0x00`).

Confirmed from captures of the RA60's own panel: pressing CH1 sends
`38 88 88 ..`, CH2 `83 88 88 ..`, CH3 `88 38 88 ..`, CH4 `88 83 88 ..`.
(An earlier version of this document and library had the order reversed,
which drove outputs 1/4 when 2/3 were intended.)

The panel sends `20 00` in bytes 8-9 of full-state commands and `88 88` in
momentary press frames; the meaning is unknown. This library sends `00 00`,
which the RA60 accepts and acknowledges.

| Nibble value | Meaning |
|---:|---|
| 0 | off |
| 1 | on |
| 2 | off (momentary) |
| 3 | on (momentary) |
| 4 | off (strobe) |
| 5 | on (strobe) |
| 6 | unverified |
| 7 | unverified |
| 8 | ignore this channel (leave its current state unchanged) |

**Why the library only uses 0, 1, 5, and 8**: the controller exposes three
output *modes* (standard, momentary, strobe), each with its own on/off pair,
but only three effects are physically observable: **off** (0, 2, and 4 are
all equivalent), **steady on** (1 and 3 are equivalent), and **strobe** (5 is
the only "on" value that differs -- it makes the output blink instead of
holding steady). So a client only ever needs to send 0 (off), 1 (on), 5
(strobe), and 8 (ignore) to reach every physically distinct state; 2, 3, and
4 are redundant for control purposes, and 6/7 remain unverified.

## Timing

At 2400 baud, an 11-byte frame takes roughly 11 x 10 bits / 2400 baud ≈ 46ms
to transmit or receive in full. Anything that polls for a received frame
faster than that (this library's `auxbeam_light_process()` is commonly
called every 10ms) will routinely see a **partial** frame on any given
poll -- see [ARCHITECTURE.md](ARCHITECTURE.md#frame-reassembly) for how the
library handles that.

## Open questions

This protocol was reverse-engineered from observed traffic, not from a
vendor specification. Treat the following as unverified:
- Nibble values 6 and 7 (never observed).
- The exact meaning of the "momentary" mode beyond its steady on/off effect
  being identical to the standard mode's.
- Byte 3's `0x00`/`0x01` values for "master on/off" commands (the library
  doesn't implement master power control, only per-output commands).
