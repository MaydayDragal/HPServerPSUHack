# PL30 secondary Rev.06 20A CC firmware reconstruction

Editable XC16/MPLAB-style behavioral reconstruction of the modified PL30 Rev.06 secondary-side firmware labelled **20A CC**.

Start with:

- `src/main_reconstructed.c`
- `src/pl30sec_symbols.h`
- `analysis/patch_summary_20a_cc_vs_stock_rev06.md`
- `analysis/program_memory_diff_vs_stock_rev06.csv`
- `analysis/cc_mode_patch_points.md`

The modified image differs from stock by six program-memory words total, mirrored across low and high application images.
