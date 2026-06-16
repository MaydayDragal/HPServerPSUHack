# Flashable hex: 14.5 V + load-tracking fan (PL30 secondary, PICO firmware)

`DSPIC33FJ64GS606_PICO_PL30_14V5_70A_fanfix.hex`

Built by `build_pl30_14v5_70a_fanfix_hex.py` from your existing modified-PICO
secondary image. It is **not** a from-scratch rebuild — it is your known-working
PICO binary with three surgical edits, so everything else is byte-identical.

> Not OEM source. Validate on an isolated, current-limited bench before
> connecting a battery or real load. 14.5 V × 70 A ≈ 1015 W, near the 1200 W rating.

## What changed (exact program words)

| Addr | Old | New | Purpose |
|---|---|---|---|
| `0x39B4` / `0x95B4` | `246C33` | `246E83` | Output-command float `0x46C3→0x46E8` → ~14.5 V (both images) |
| `0x4FE0` | `FFFFFF` | `FFFFAD` | Low-app checksum compensation (`-0x52`, padding word) |
| `0x5B30..0x5B4C` | fixed-current helper | load-tracking helper | Fan reacts to load (low image) |
| `0xA270..0xA28C` | fixed-current helper | load-tracking helper | Fan reacts to load (high image) |

### Fan helper, before vs after

The old helper wrote a **fixed** synthetic current (`0x00DE`/`0x00CC`) whenever
the output was enabled, so the firmware always saw "no load" and the fan never
ramped for current. The new helper writes a current shadow proportional to the
commanded CC value (`0x08C4 >> 6`), so the fan's load logic sees real load. The
stock temperature path is untouched and still works.

```
enabled branch (new):
  MOV 0x8C4, W1      ; commanded CC counts (host: amps * 265)
  LSR W1, #6, W1     ; scale into the current-shadow domain
  MOV W1, 0xEAA
  MOV 0x8C4, W2
  LSR W2, #6, W2
  MOV W2, 0xEAC
  RETURN
```

## Why this is boot-safe

The secondary bootloader checksum-validates **only the low app** (additive
byte-sum over `0x0104..0x01FF` and `0x0400..0x4FFF` must net to zero; selector at
`0x5552`). The high app at `0x6000` is an unconditional fallback and is not
gated.

- The voltage edit at `0x39B4` adds `0x52` to that byte-sum; the `0x4FE0` padding
  edit subtracts `0x52`. Net change = 0 → low app still validates. The build
  script verifies the byte-sum is identical before/after (`CHECKSUM PRESERVED`).
- The fan edits at `0x5B30` are outside the checksummed range.
- High-app edits (`0x95B4`, `0xA270`) need no compensation.

## Flashing

Program the whole secondary dsPIC33FJ64GS606 over ICSP, the same way you flash
your current PICO image:

- PICkit 3/5 (or equivalent) on the secondary ICSP header (PGC2/PGD2 per the
  config bits, `ICS = PGD2`).
- Erase + program the full device with this `.hex`.
- Power the board from standby (or the programmer) for the write.

If the unit comes up and runs (output present, fan reacts), the low-app
checksum is good. If it ever sat stuck not running, the checksum would have
failed and it would fall back to the high app at 12 V — that is your tell.

## Runtime: command 70 A CC

70 A is host-side (no firmware change). From the Pico:

```python
from pl30_test_setup import main
main()      # sets CC, enables output, ramps to 70 A
```

or directly:

```python
from pl30_pico_host import PL30
psu = PL30()
psu.set_current(70.0)   # 18550 counts
psu.enable(cc=True)
```

## Calibrate after flashing (in this order)

1. **Voltage**: into a light load, measure output. Expect ~14.5 V. To trim,
   change the `0x39B4`/`0x95B4` high word a few counts and re-balance `0x4FE0`
   by the opposite byte delta (or just re-run the build script with an adjusted
   constant — keep the checksum line `CHECKSUM PRESERVED`).
2. **CC scale**: command a low current (e.g. 10 A), measure actual amps, confirm
   the 265 counts/A mapping before trusting 70 A.
3. **Fan scale**: watch the fan under a load sweep. If it ramps too eagerly or
   nuisance-trips OCP, change the two `LSR …,#6` to `#7` (`DE08C6→DE08C7`,
   `DE1146→DE1147`) and re-run the build. If it stays lazy under load, use `#5`.

## Reproducing / editing

`build_pl30_14v5_70a_fanfix_hex.py` is self-contained: edit the `PATCH` dict and
re-run. It re-emits valid Intel HEX with correct record checksums and prints the
low-app byte-sum invariant check.
