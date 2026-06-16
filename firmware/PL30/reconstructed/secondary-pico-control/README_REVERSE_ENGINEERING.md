# PL30 Secondary PICO/RPi-Control Firmware Reconstruction

This package reconstructs the modified PICO/RPi-control PL30 Rev.10 secondary firmware for the dsPIC33FJ64GS606.

Main files:

- `analysis/pico_control_quickstart.md` - **start here**: wiring, I2C wire format, CC range, step-by-step first use.
- `tools/pl30_pico_host.py` - MicroPython class for Pico to control the PSU (set CC, enable/disable, button demo).
- `src/main_reconstructed.c` - base secondary reconstruction with PICO-control patch points annotated.
- `src/pico_control_hooks_reconstructed.c` - C equivalents of inserted helper blocks at `0x5B00..0x5B76` and `0xA220..0xA2C6`.
- `src/pico_control_patch_points.h` - editable register IDs, scale factors, bit masks, and synthetic ADC bias values.
- `src/pl30sec_symbols.h` - address-based RAM/SFR symbol map with PICO variables added.
- `analysis/pico_control_patch_summary.md` - detailed explanation of the behavior changes.
- `analysis/pico_control_patch_reference.asm` - exact address-level assembly patch reference.
- `analysis/program_memory_diff_vs_stock_rev10.csv` - exact word-level diff against stock Rev.10.
- `analysis/program_memory_diff_groups_vs_stock_rev10.csv` - grouped diff view.
- `original_modified_pico/` - uploaded modified image/dumps.
- `stock_rev10_reference/` - stock Rev.10 reference dumps.
- `nohwmod_reference/` and `rev06_cc_reference/` - contextual comparison inputs when available.
- `github_context/` - notes from referenced GitHub projects.

Summary:

- PICO vs stock Rev.10 program-word differences: 120.
- Config-bit differences vs stock Rev.10: 0.
- Added external writable I2C logical registers: `0x1F` for CC setpoint and `0x21` for status/control.
- Current setpoint scale used by zpsu/Pico: 265.0 counts/A.
- OVP ADC scaling helper: `(ADCBUF2 * 0x6AC2) >> 15`, about 0.834045.

Not OEM source; validate on isolated current-limited hardware before flashing.
