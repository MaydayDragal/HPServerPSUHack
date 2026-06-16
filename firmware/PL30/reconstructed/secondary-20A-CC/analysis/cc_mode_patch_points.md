# Editable 20A CC patch points

| Low image | High image | Stock | 20A CC | Meaning |
|---:|---:|---:|---:|---|
| 0x3348 | 0x8F48 | 0x0370 | 0x03B2 | Filtered VOUT upper/protection threshold |
| 0x3446 | 0x9046 | 0x5511 | 0x1000 | Case-1 current/CC loop constant written to 0x0E50 |
| 0x39B4 | 0x95B4 | high word 0x46C3 | high word 0x46E8 | Output command float constant, low word 0xEF9E unchanged |

For binary patches, edit both the low image and high mirror image because the boot selector can enter either copy.
