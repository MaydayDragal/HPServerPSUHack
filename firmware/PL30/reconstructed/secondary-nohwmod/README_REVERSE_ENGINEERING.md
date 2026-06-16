# PL30 secondary-side NoHWMod firmware reconstruction

Target: **dsPIC33FJ64GS606** secondary-side controller from the NoHWMod modified PL30 Rev.10 dumps.

This directory contains an editable XC16/MPLAB-oriented reconstruction of the modified secondary IC image, plus a stock-vs-modified comparison. It is not original source and is not expected to compile into a bit-identical binary without a custom linker script, vector table, startup selection, and additional hand verification.

## Contents

- `src/main_reconstructed.c` — reconstructed NoHWMod application startup, main loop, ISR-level behavior, and edited control logic.
- `src/bootloader_reconstructed.c` — reconstructed boot/update selector at `0x5000`; unchanged except for the added low-image helper in nearby blank flash.
- `src/pl30sec_symbols.h` — named RAM and SFR symbols inferred from the dumps.
- `src/app_initial_data.h` — recovered startup constants from the C data-init tables.
- `src/config_bits.c` — NoHWMod configuration-bit pragmas; values match stock secondary config bits.
- `analysis/diff_stock_vs_nohwmod.csv` — word-level program-memory diff.
- `analysis/diff_groups.csv` — six grouped patch locations.
- `analysis/diff_summary.md` — plain-language explanation of the OVP/output-voltage change.
- `analysis/nohwmod_patch_points.md` — editable patch-point notes.
- Other `analysis/*.csv` files — regenerated maps and aids for the modified image.
- `original/modified_nohwmod/` — uploaded modified dumps.
- `original/stock_reference/` — uploaded stock secondary reference dumps used for the diff.

## Main finding

The NoHWMod image changes only 30 program-memory words versus stock. The changes add a Q15 ADC scaling helper and route the output-voltage ADC ISR through it. The helper computes:

```c
scaled_vout = ((uint32_t)ADCBUF2 * 0x6AC2u) >> 15;
```

That scale is `27330 / 32768 = 0.83404541015625`, so the firmware sees roughly 83.4% of the original output-voltage feedback count. For unchanged thresholds that consume `0x0A00`/`0x0A02`, the corresponding physical voltage increases by approximately `32768/27330 = 1.19898`, or about +19.9%.

A second mirrored one-word patch changes the high half of an IEEE-754 float literal from `0x46C3EF9E` to `0x46E4EF9E`, raising that float from `25079.8086` to `29303.8086` in the PWM4/output-command threshold computation path.

## Safe workflow

1. Keep the stock and NoHWMod HEX files untouched as golden references.
2. Import `src/` into a new MPLAB X dsPIC33FJ64GS606/XC16 project.
3. Verify all config-bit pragma names with your XC16 device pack.
4. Scope PWM3/PWM4/PWM6 and ADC trigger timing before and after any change.
5. Test with isolation, a current-limited input source, and a dummy load before connecting the real power stage.
