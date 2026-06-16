# PL30 secondary-side firmware reconstruction

Target: **dsPIC33FJ64GS606** secondary-side controller from stock PL30 Rev.10 dumps.

This directory contains an editable XC16/MPLAB-oriented reconstruction.  It is not original source and is not expected to compile into a bit-identical binary without a custom linker script, vector table, startup selection, and additional hand verification.

## Contents

- `src/main_reconstructed.c` — reconstructed application startup, main loop, ISR-level behavior, and editable control logic.
- `src/bootloader_reconstructed.c` — reconstructed boot/update selector at `0x5000`.
- `src/pl30sec_symbols.h` — named RAM and SFR symbols inferred from the dumps.
- `src/app_initial_data.h` — recovered startup constants from the C data-init tables.
- `src/config_bits.c` — stock configuration-bit pragmas.
- `analysis/function_map.csv` — key function map with low/high app duplicate addresses.
- `analysis/known_variables.csv` — inferred RAM map.
- `analysis/interrupt_vectors.csv` — normal/high and alternate/low vector targets.
- `analysis/data_init_records.csv` — decoded startup data records.
- `analysis/call_edges.csv`, `analysis/call_targets_summary.csv`, `analysis/program_memory_ranges.csv` — raw analysis aids.
- `analysis/reverse_engineering_notes.md` — human-readable notes.
- `original/` — copies of uploaded source dumps for traceability.

## Safe workflow

1. Import `src/` into a new MPLAB X dsPIC33FJ64GS606/XC16 project.
2. Keep the stock HEX untouched as your golden reference.
3. Verify all config-bit pragma names with your XC16 device pack.
4. Scope PWM3/PWM4/PWM6 and ADC trigger timing before and after any change.
5. Test with isolation, a current-limited input source, and a dummy load before connecting the real power stage.
