# PL30 Firmware Reconstructions

Source-level (XC16/MPLAB-style) reconstructions of the HP HSTNS-PL30 power-supply
firmware, recovered from on-chip program-memory dumps. These are **editable
behavioral reconstructions, not bit-identical OEM source** — they exist to make the
control flow, startup defaults, safety interlocks, communication protocol, and
calibration paths readable and modifiable.

The PL30 uses two controllers that talk to each other across an ADuM2201 digital
isolator:

- **Primary** — dsPIC33FJ16GS502 (PFC/boost + line metering)
- **Secondary** — dsPIC33FJ64GS606 (output regulation, protection, host I²C/UART)

## Packages

| Directory | Chip | Image | What it is |
|---|---|---|---|
| `primary-dsPIC33FJ16GS502/` | dsPIC33FJ16GS502 | Stock Rev.10 | Primary-side controller: PFC sine-envelope control, PWM1/PDC1 duty, comparator current limit, RMS/energy math, and the host UART protocol. |
| `secondary-stock/` | dsPIC33FJ64GS606 | Stock Rev.10 | Baseline secondary reconstruction: startup, main loop, ISRs, bootloader/update selector. |
| `secondary-20A-CC/` | dsPIC33FJ64GS606 | Rev.06 20 A CC | Constant-current (AGM charger) mod. Differs from stock by **6 program words**. |
| `secondary-nohwmod/` | dsPIC33FJ64GS606 | Rev.10 NoHWMod | 14.4 V output with no board rework. Differs from stock by **30 program words**. |
| `secondary-pico-control/` | dsPIC33FJ64GS606 | Rev.10 Pico-control | Adds external I²C control (CC setpoint / status) for the RPi Pico watt-meter. Differs from stock by **120 program words**. |

Each package contains:

- `src/` — reconstructed C plus a symbol map (`pl30sec_symbols.h` / equivalent).
- `analysis/` — function maps, call graphs, interrupt-vector tables, inferred RAM
  maps, decoded data-init records, exact program-memory diffs, and
  `reverse_engineering_notes.md`.
- `original/` (and `*_reference/` dirs) — the raw program-memory / config-bit / SFR
  / file-register dumps the reconstruction was derived from, kept for traceability.

## Key findings

**Host UART protocol (primary ↔ secondary) is now decoded.** The primary's
`send_status_frame` builds frames as `0xEA + status + checksum`
(checksum = `0x100 − sum`), and `uart_rx_isr` is the matching state machine
(start `0xEA`, ACK `0x18`, checksum). This is exactly the `EA 18 18 18 EA 18 …`
startup stream captured in the project README. The secondary sends the other half:
`0x05`-query and `0x50`-param frames. Host commands land in a 16-bit register window
at primary RAM `0x0BC8`, with command addresses `0x20` (output gate) and `0x28`
(setpoint → blanking/delay).

**External I²C control registers (Pico package).** Two writable logical registers
were added for the RPi Pico / zpsu interface: `0x1F` (CC setpoint) and `0x21`
(status/control), with a current-setpoint scale of **265.0 counts/A**.

**Output-voltage / OVP scaling.** The NoHWMod and Pico images share the ADC scaling
hook `(ADCBUF2 * 0x6AC2) >> 15 ≈ ×0.834`, which makes the firmware read output
voltage low so unchanged thresholds trip at a higher physical voltage — the same
0.834 constant documented in the project README.

## Provenance and safety

Reconstructed from program-memory dumps with AI assistance; first-pass, not OEM
source, and not guaranteed to rebuild bit-identically without a custom linker
script, vector table, and startup stubs. Before flashing any modified image, work on
isolated, current-limited hardware with a dummy load and scope the PWM/ADC timing.

Bundled copies of the Microchip device datasheets that shipped inside these packages
were removed to keep the repository lean; the relevant datasheets are under the
top-level [`datasheet/`](../../../datasheet/) directory.
