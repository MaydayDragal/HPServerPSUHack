# PL30 Rev.06 Secondary 20A CC / ~14.4 V patch summary

Target: dsPIC33FJ64GS606 secondary-side firmware.

The modified image differs from the supplied stock Rev.06 secondary image by exactly **6 program-memory words**, mirrored across the low and high application images.  The stock Rev.06 and stock Rev.10 secondary images were previously found to be payload-identical, so these are effectively differences versus stock Rev.10 as well.

## Patch points

1. `0x3348` / `0x8F48`: `MOV #0x0370,W0` -> `MOV #0x03B2,W0`.  This raises a filtered VOUT upper/protection threshold from 880 to 946 counts, +7.500%.
2. `0x3446` / `0x9046`: `MOV #0x5511,W0` -> `MOV #0x1000,W0`.  This changes the case-1 loop/filter constant written to RAM `0x0E50` to 0.188088 of the stock value while leaving the case-1 base current-loop target `0x1860` and phase/floor `0x12C0` unchanged.
3. `0x39B4` / `0x95B4`: `MOV #0x46C3,W3` -> `MOV #0x46E8,W3`, with low word `0xEF9E` unchanged.  The 32-bit float constant changes from `0x46C3EF9E` = 25079.80859375f to `0x46E8EF9E` = 29815.80859375f, +18.884%.

The practical behavior is a firmware-only shift toward a higher output-voltage command plus a retuned current-loop profile consistent with 20 A CC behavior.  Exact output current still depends on calibration and hardware sensing.
