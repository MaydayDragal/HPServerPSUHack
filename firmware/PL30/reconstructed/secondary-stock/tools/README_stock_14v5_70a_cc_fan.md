# Flashable hex: STOCK Rev.10 + 14.5 V + 70 A CC + current-driven fan

`DSPIC33FJ64GS606_STOCK_PL30_14V5_70A_CC_fan.hex`

Built by `build_stock_14v5_70a_cc_fan.py` from the **stock Rev.10** secondary
image (not the pico build). The 20A-CC image is just stock Rev.10 + 3 constants,
so "porting the CC loop" reduces to applying those constants at a 70 A setting —
plus a fan fix for the CC-mode-inherent idle-until-OTP problem.

> Not OEM source. This has NOT been hardware-validated. Simulate in MPLAB X and
> test into a current-limited electronic load before any battery/real load.
> 14.5 V x 70 A ~= 1015 W, near the 1200 W rating.

## What changed

| Addr (low / high) | Old | New | Purpose |
|---|---|---|---|
| `0x39B4` / `0x95B4` | `246C33` | `246E83` | Output float `0x46C3->0x46E8` -> ~14.5 V |
| `0x3348` / `0x8F48` | `203700` | `203B20` | OVP/VOUT threshold `0x370->0x3B2` (as 20A-CC) |
| `0x3446` / `0x9046` | `255110` | `238000` | Case-1 CC limit (`0x0E50` source) `0x5511->0x3800` (~70 A) |
| `0x2DDA` / `0x89DA` | `8044B1` | `RCALL 0x5B68` | Fan curve load input -> current helper |
| `0x5B68..0x5B76` | padding | helper | Current-driven fan helper (free flash) |
| `0x4FE0` | `FFFFFF` | `FFFFFE` | Low-app checksum rebalance |

## The fan fix (why both prior CC builds idled to OTP)

The stock fan's load-based ramp is driven by an output-**power** estimate that
the firmware (a) only computes in the run state (app state 3) and (b) derives
from output **voltage** (`0x0A00`). In CC mode the output voltage is held below
the CV setpoint, so the estimate stays low and the fan reads "light load" — even
at 70 A. This is inherent to CC operation, which is why the pico build *and* the
20A-CC-at-70A build behaved identically.

The fix redirects the fan curve's load input from the voltage-weighted metric
(`0x0896`) to **filtered output current** (`0x0E54`). A 4-instruction helper in
free flash reads `0x0E54`, compares it to a threshold, and returns the exact
hi/lo value the stock curve already compares against (`0xFFFF` or `0x0000`), so
**no curve thresholds move** and the in-checksum footprint is a single `RCALL`.

```
0x5B68  MOV 0x0E54, W1     ; filtered output current
0x5B6A  MOV #0x40, W0      ; threshold (0x0E54 domain) -- TUNABLE, free flash
0x5B6C  SUB W1, W0, [W15]
0x5B6E  BRA LEU, 0x5B74    ; current <= threshold -> return 0 (no ramp)
0x5B70  MOV #0xFFFF, W1    ; current >  threshold -> return 0xFFFF (ramp)
0x5B72  RETURN
0x5B74  CLR W1
0x5B76  RETURN
```

The stock temperature path (`0x2E86`) is untouched and remains as a backstop.

## Why it's boot-safe

The bootloader validates only the low app, by an additive byte-sum over
`0x0104..0x01FF` and `0x0400..0x4FFF` that must net to zero (selector `0x5552`).
The high app at `0x6000` is an unconditional fallback. The build script keeps
the low-app byte-sum **exactly invariant** (compensating `0x4FE0`) and asserts it
before writing — a wrong checksum cannot ship. The fan helper sits at `0x5B68`,
above `0x4FFF`, so it is outside the checksummed range.

## Flashing

Program the whole secondary dsPIC33FJ64GS606 over ICSP (PICkit, `ICS = PGD2`),
the same way you flash any secondary image. Erase + program full device.

## Calibrate after flashing (in this order, into a current-limited load)

1. **Voltage** — measure output; expect ~14.5 V. Trim by re-running the build
   with an adjusted `0x39B4`/`0x95B4` high word (script rebalances the checksum).
2. **CC limit (70 A)** — `0x3800` is anchored to the 20A-CC calibration point
   (20 A -> `0x1000`), so it is an estimate. Command/observe a low current,
   measure actual amps, and scale `0x3446` (`build` `CONST_PATCH`) accordingly.
3. **Fan threshold** — `0x0040` in the `0x0E54` current domain is a low,
   safe-biased default (fan ramps readily). The threshold lives in **free flash
   at `0x5B6A`**, so you can change it with no checksum rebalance:
   - Fan runs even at idle -> raise it (e.g. `MOV #0x80,W0` = `200800`).
   - Fan lazy under load -> lower it.
   Read `0x0E54` over ICSP at known currents to calibrate precisely. Note the
   helper does not scale `0x0E54`; if your filtered current can exceed ~`0x7FF`
   the threshold still works (no overflow — it's a direct compare).

## Reproducing / editing

`build_stock_14v5_70a_cc_fan.py` is self-contained and self-verifying: it asserts
every source opcode matches before patching, auto-places the helper for a
compensatable checksum delta, and asserts the low-app byte-sum is invariant.
Edit `CONST_PATCH` / `FAN_CURRENT_THRESHOLD` and re-run.
