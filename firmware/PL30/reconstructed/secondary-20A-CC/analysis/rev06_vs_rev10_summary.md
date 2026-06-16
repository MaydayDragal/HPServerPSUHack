# PL30 stock Rev.06 secondary vs stock Rev.10 secondary comparison

## Result

The uploaded stock Rev.06 secondary firmware is functionally the same binary image as the stock Rev.10 secondary firmware previously reconstructed.

Measured differences:

| Item compared | Result |
|---|---:|
| Program-memory words parsed from each dump | 22016 Rev.06 / 22016 Rev.10 |
| Program-memory word differences | 0 |
| Config-bit text-line differences | 0 |
| Decoded Intel HEX byte differences | 0 |
| Raw HEX file bytes identical | False |
| HEX identical after CRLF/LF newline normalization | True |
| File-register snapshot differences | 2 |
| SFR snapshot differences | 2 |

## Interpretation

I found no code, config-bit, or decoded HEX payload difference between stock Rev.06 secondary and stock Rev.10 secondary.  The same reconstructed C source used for stock Rev.10 therefore applies to stock Rev.06.

The only non-zero differences I found are in debug/runtime snapshot dumps, not firmware:

- `File Registers`: `PCL` and `SR` differ by a small amount.
- `SFRs`: `PCL` and `SR` differ by a small amount, in the opposite snapshot direction.
- Raw HEX SHA-256 differs because the Rev.10 HEX file uses CRLF line endings while the Rev.06 HEX file uses LF line endings.  After newline normalization and full Intel HEX decoding, the payload bytes are identical.

## Practical takeaway

There does not appear to be a secondary-side firmware change between the stock Rev.06 and stock Rev.10 files supplied here.  Any Rev.06-to-Rev.10 PL30 behavior change is therefore more likely to be elsewhere: primary firmware, hardware/BOM, calibration data, EEPROM contents, manufacturing trim, or external system configuration.
