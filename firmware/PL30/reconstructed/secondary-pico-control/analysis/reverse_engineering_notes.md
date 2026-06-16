# PICO/RPi-Control PL30 Rev.10 Secondary Firmware Reconstruction

Target: dsPIC33FJ64GS606 secondary-side controller.

This package is a behavioral C reconstruction of the modified `PICO PL30 Rev.10 Sec` image, built by comparing it against the previously supplied stock PL30 Rev.10 secondary image.

## High-level result

The PICO image is stock Rev.10 plus targeted hooks. It is not a rewrite of the main PSU control loop.

- Program-memory words parsed: 22016 PICO / 22016 stock Rev.10.
- Program-memory word differences vs stock Rev.10: **120**.
- Program-memory diff groups vs stock Rev.10: **19**.
- Config-bit differences vs stock Rev.10: **0**.
- Decoded Intel HEX byte differences vs stock Rev.10: **356** across **120** contiguous byte groups.

## What makes Raspberry Pi/Pico control possible

The stock secondary firmware already has an I2C1 register-command protocol. The PICO patch adds two previously-unused writable command-table entries and uses them to expose RAM variables to an external controller:

| External logical reg | Wire reg | dsPIC destination | Meaning |
|---:|---:|---:|---|
| `0x1F` | `0x3E` | RAM `0x08C4` | Constant-current command in counts; zpsu uses `amps * 265.0`. |
| `0x21` | `0x42` | RAM `0x08C8` | Status/control word; bit15 output enable, bit0 CC enable. |

The table entry at `0x0D38` now branches to the low-image CC write helper at `0x5B26`; the high-image mirror at `0x6938` branches to `0xA260`. The table entry at `0x0D40` now branches to the low-image status write helper at `0x5B20`; the high-image mirror at `0x6940` branches to `0xA250`.

The external CC value becomes active because the patch also hooks the existing current-limit/mode-table path. At `0x3462` and high mirror `0x9062`, stock instructions are replaced by calls to the inserted helpers at `0x5B50`/`0xA2A0`. When status bit0 is set, the helper clamps RAM `0x08C4` to the range `0x0500..0x0E50`, writes the clamped value back to `0x08C4`, and then writes it into RAM `0x0E50`, which is already consumed by the original current-limit path.

That is the key firmware trick: the patch does not implement a full new digital CC loop. It makes an external I2C register write feed the existing current-limit machinery.

## Additional behavior changes

### 1. OVP/output-voltage sense scaling

The PICO image includes the same OVP sense-scaling helper as the earlier NoHWMod image:

```c
scaled_vout_adc = ((uint32_t)ADCBUF2 * 0x6AC2u) >> 15;
```

The decimal scale is `0.834045410`, so the secondary MCU sees about 83.4% of the raw output-voltage ADC count. This aligns with the documented 220 ohm to 182 ohm divider change and keeps OVP logic referenced to the original apparent voltage.

Unlike the earlier NoHWMod Rev.10 image, this PICO image does **not** change the output setpoint float at `0x39B4/0x95B4`; it remains stock `0x46C3EF9E`.

### 2. Current ADC substitute hook

At `0x2BD6` and high mirror `0x87D6`, the original raw `ADCBUF6`/`ADCBUF7` copy is replaced by helper calls. The helper writes fixed current shadow words:

- output disabled: `0x0EAA = 0x03A3`, `0x0EAC = 0x0000`;
- output enabled: `0x0EAA = 0x00DE`, `0x0EAC = 0x00CC`.

This appears to condition the existing current/status path around the externally controlled output-enable state.

## Patch-point files

- `src/pico_control_hooks_reconstructed.c` contains C equivalents of the inserted helper blocks.
- `src/pico_control_patch_points.h` contains the edit-friendly constants.
- `analysis/pico_control_patch_reference.asm` is the exact address-level assembly reference.
- `analysis/program_memory_diff_vs_stock_rev10.csv` and `analysis/program_memory_diff_groups_vs_stock_rev10.csv` provide the complete word-level diff.

## Validation note

This is not bit-identical OEM source. It is a readable/editable reconstruction designed for MPLAB/XC16-oriented analysis and future patching. Validate all changes on an isolated, current-limited bench setup before using a live PSU or battery/load.


---

See the stock reconstruction notes in the stock Rev.10 package for peripheral initialization and the unmodified control-state-machine overview.
