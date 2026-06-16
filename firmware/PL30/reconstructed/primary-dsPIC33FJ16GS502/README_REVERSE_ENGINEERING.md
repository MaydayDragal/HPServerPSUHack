# PL30 primary-side dsPIC33FJ16GS502 firmware reconstruction

This package is a first-pass editable XC16/MPLAB reconstruction of the stock **PL30 Rev.10 primary-side** firmware image. It is intended for reverse engineering and controlled firmware modification, not as a verified drop-in production replacement.

## What was reconstructed

- Reset/startup path: reset vector jumps to `0x0200`; C startup initializes stack/SPLIM and data, then calls the application at `0x1AB8`.
- Configuration bits: preserved in `src/config_bits.c` from the stock config dump.
- Peripheral init: clock/PLL, GPIO, Timer1, UART/PPS, ADC pair triggers/interrupts, comparator 1, PWM1 defaults.
- Main state machine: four-state control loop recovered from `0x1AB8..0x1D32`.
- PWM behavior: PWM1 uses `PHASE1`, `PDC1`, and `TRIG1`; `PDC1` is computed in the ADC ISR path and PWM is disabled in trap handlers.
- Host protocol: UART frames begin with `0xEA`; ACK is `0x18`; status replies contain status bytes and an 8-bit checksum.
- Data tables: 373-entry envelope/sine table recovered from data RAM `0x0816..0x0AFE`.

## Important limitations

1. **Original symbol names and comments cannot be recovered from HEX/disassembly.** Names such as `G_POWER_TARGET` and `G_CONTROL_FLAGS` are inferred from behavior.
2. **This C is not bit-identical to the original binary.** Several compiler library routines and some control math are condensed into readable C. The files are designed to make behavior understandable and editable; they are not yet a binary-equivalent decompilation.
3. **Interrupt vector names still need verification in your XC16 device header.** The reconstructed ISR bodies are provided as neutral `pl30_*_isr_body_*()` functions so the wrong vector is not accidentally bound.
4. **Power-stage validation is mandatory.** Do not flash into a mains-connected PSU. Validate on an isolated supply/current-limited fixture with dummy load, differential probes, and a way to recover/program the IC.

## MPLAB X import

1. Create a new **Standalone Project**.
2. Device: `dsPIC33FJ16GS502`.
3. Compiler: XC16.
4. Add all files under `src/`.
5. Keep `src/config_bits.c` in the build so the stock configuration is reproduced.
6. Start with simulator or bench test. For waveform testing, compare PWM period, duty ramp, comparator DAC, and UART status against the stock image before changing limits.

## Likely behavior-control locations

- `G_HOST_SETPOINT` / `G_HOST_DELAY` (`0x0BF0/0x0BF2`): host command address `0x28` changes a derived delay/blanking value. Minimum is forced to `0x1E`.
- `G_PWM_PERIOD_REF`, `G_PWM_PERIOD_TARGET`, `G_PWM_PERIOD_ACTIVE` (`0x0C0E/0x0C10/0x0C12`): period/ramp values for PWM behavior.
- `CMPDAC1`: set to `0x03FF` normally and `0x0294` in one range-select branch; this is likely a fast current/voltage limit threshold.
- `PDC1` / `TRIG1`: duty and ADC trigger are updated together in the ADC ISR path.
- `G_STATUS_FLAGS` (`0x0BC8`) and `G_FAULT_FLAGS` (`0x0BE8`): main state machine gating, host-visible status, inhibit/fault latching.
- `PL30_SINE_TABLE`: 373-point modulation/envelope table used every main loop to produce `G_ENV_A` and `G_ENV_B`.

## File list

- `src/main_reconstructed.c` — editable reconstructed firmware logic.
- `src/pl30_symbols.h` — RAM/SFR aliases and inferred flag names.
- `src/pl30_sine_table.h` — recovered 373-entry table.
- `src/config_bits.c` — stock configuration pragmas.
- `analysis/function_map.csv` — function entry points and inferred roles.
- `analysis/known_variables.csv` — important RAM variables and inferred roles.
- `original/` — copies of the uploaded stock text dumps used for traceability.

## Recommended reverse-engineering next steps

1. Compile this in MPLAB without flashing; fix any XC16 header naming differences.
2. Bind the ISR bodies to actual vector names one at a time and compare SFR writes in the simulator.
3. Scope stock hardware behavior: PWM1H/1L, comparator trip, ADC trigger timing, UART traffic.
4. Patch one scalar at a time, preferably through variables rather than changing control topology.
5. Keep an unmodified IC or full programming backup available for recovery.
