# Bench setup: 14.5 V output, 70 A CC, load+temperature fan response

This describes the changes needed to run a PL30 (PICO secondary firmware) at
14.5 V with a 70 A constant-current limit, with the fan responding to load and
temperature again.

Three pieces are involved, in two places:

| Goal | Where it lives | Mechanism |
|---|---|---|
| 70 A CC limit | **Host (Pico)** | I2C register `0x1F`, runtime — `tools/pl30_test_setup.py` |
| 14.5 V output | **Firmware** | Output-command float at `0x39B4`/`0x95B4` |
| Fan reacts to load+temp | **Firmware** | Make the current-substitute helper track commanded load |

> Not OEM source. Validate every change on an isolated, current-limited bench
> setup before connecting a battery or real load. 14.5 V × 70 A ≈ 1015 W — close
> to the 1200 W rating.

---

## 1. 70 A constant current (host side, no flashing)

`70 A × 265 counts/A = 18550 = 0x4876`. This clears the firmware CC floor
(`0x0500` = 1280 counts ≈ 4.83 A) and sits under the mode-table ceiling at RAM
`0x0E50`.

Run `tools/pl30_test_setup.py`. It sets the CC limit and enables the output in
CC mode, ramping up so you can watch a meter.

**Confirm the scale before trusting 70 A.** The 265 counts/A figure is the
zpsu/Pico convention; the dsPIC compares the command against its internal limit
at `0x0E50`. Command a *low* current (e.g. 10 A), measure actual output amps,
and compute the true counts/A. Adjust `PICO_CONTROL_CURRENT_SCALE_COUNTS_PER_AMP`
in `pl30_pico_host.py` if it differs. If 70 A gets clamped lower than expected,
the mode-table ceiling source constant (written to `0x0E50` near `0x3448`) is the
limiter and must be raised in firmware.

---

## 2. 14.5 V output (firmware)

The output regulation reference is an IEEE-754 float embedded at program address
`0x39B4` (low image) and mirrored at `0x95B4` (high image). Only the **high
word** changes; the low word `0xEF9E` is unchanged.

| Image addr | Stock high word | New high word | Full float | Approx reference |
|---|---|---|---|---|
| `0x39B4` / `0x95B4` | `0x46C3` | `0x46E8` | `0x46E8EF9E` | ×1.189 vs stock |

`0x46C3EF9E ≈ 25080` (reference counts for the stock ~12.2 V point);
`0x46E8EF9E ≈ 29816`, a 18.9 % increase → roughly 14.5 V. This is the same
constant the NoHWMod and 20A-CC builds use to raise output voltage.

**The exact volts depend on this unit's hardware divider and EEPROM
calibration.** The PICO build already applies the OVP/sense scaling
`(ADCBUF2 × 0x6AC2) >> 15 ≈ ×0.834`, which assumes the 220 Ω→182 Ω feedback
divider mod is present. So measured output = f(float, divider, per-unit cal).
Procedure:

1. Patch `0x39B4` and `0x95B4` from `0x46C3` to `0x46E8` (edit **both** — the
   boot selector can run either image).
2. Flash, power up into a light load, measure output.
3. Trim the high word up/down a few counts to land on 14.5 V exactly. Each LSB
   of the high word's low bits is a small fraction of a volt; iterate.

Recompute the float if you want a precise target:
`float_value = desired_reference_counts`, then encode IEEE-754. The
reference-counts-to-volts gain is what you establish by measuring in step 2.

---

## 3. Fan: restore load + temperature response (firmware)

### Why it broke

The PICO patch replaced the current-ADC read in the current ISR (`0x2BD6`) with
a helper (`0x5B30`) that writes a **fixed** synthetic current
(`0x00DE`/`0x00CC` when enabled). That was done to keep the stock over-current
path from fighting external CC — but it tells the firmware "almost no load" at
all times, so the fan's load-driven ramp never fires. Only the (slower,
higher-threshold) temperature branch remained, which is why the unit cooked to
OTP instead of spinning up. The fan control code itself is unmodified stock.

### The fix: make the synthetic current track commanded load

Replace the fixed constants with a value proportional to the commanded CC
setpoint (`0x08C4`), which is a clean, bounded number you already control. The
fan/load logic then sees representative load, while OCP sees a controlled value
that won't false-trip as long as you command within the OCP limit. The stock
temperature path is untouched and keeps working.

Replacement low-image helper at `0x5B30` (mirror the same at `0x A270`):

```asm
; 0x5B30 / 0xA270 — load-tracking current-shadow helper.
; On return, W1 and W2 hold the shadow values; the ISR continues at 0x2BDE
; and filters them into 0x0E54/0x0E56, so both raw and filtered current track
; commanded load.
5B30  BTST  0x8C9, #7        ; output-enable bit (bit15 of status word 0x08C8)
5B32  BRA   NZ, 0x5B3E
      ; --- output disabled: idle shadow (as before) ---
5B34  MOV   #0x3A3, W1
5B36  MOV   W1, 0xEAA
5B38  CLR   W2
5B3A  MOV   W2, 0xEAC
5B3C  BRA   0x5B48
      ; --- output enabled: shadow proportional to commanded CC ---
5B3E  MOV   0x8C4, W1        ; W1 = CC command counts (host: amps * 265)
5B40  LSR   W1, #6, W1       ; W1 = counts / 64  (TUNABLE scale, see below)
5B42  MOV   W1, 0xEAA        ; primary current shadow tracks load
5B44  LSR   W1, #1, W2       ; secondary shadow ~ half
5B46  MOV   W2, 0xEAC
5B48  RETURN
```

The original helper was 12 words (`0x5B30..0x5B46`); this fits the same slot.

### Tuning the shadow scale (`>>6`)

`>>6` maps 70 A (18550 counts) → 289, which lands in the stock current-shadow
domain (the stock thresholds in the current ISR are around `0x45`–`0x24C`, and
the original frozen "enabled" value was `0xDE` = 222). Bench-tune it:

1. Start with `>>6`. Flash, run a known load sweep, watch the fan.
2. Fan ramps too eagerly / OCP nuisance-trips → shift more (`>>7`).
3. Fan stays lazy under real load → shift less (`>>5`).

The goal is: at your working currents the synthesized value crosses the fan's
load-demand threshold *and* stays under the OCP trip.

### If the fan still won't ramp

The fan's load demand may key off a different ADC channel (the PWM6-loop sense
at RAM `0x0A06`/`0x0A0A`) rather than the substituted current. The current-shadow
fix is the change with the clearest link to the regression, but it is not 100%
confirmed to be the fan's load input. If the fan stays idle after this change,
use the **external supervisor** in `pl30_fan_supervisor.py`: the Pico computes a
load+temperature fan curve from data it already has (commanded current) plus
temperature it reads, and commands a fan target via a small added firmware
register. See that file's header for the register it needs.

---

## Order of operations

1. Flash the 14.5 V float change (#2) and the load-tracking fan helper (#3),
   both images.
2. Power up into a light current-limited load. Confirm ~14.5 V; trim the float.
3. Run `tools/pl30_test_setup.py`; confirm CC scale at low current, then ramp.
4. Apply real load and watch the fan ramp with current and temperature.
