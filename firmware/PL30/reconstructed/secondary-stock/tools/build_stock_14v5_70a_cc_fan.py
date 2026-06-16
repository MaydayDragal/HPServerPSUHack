#!/usr/bin/env python3
"""
Build a flashable STOCK Rev.10 secondary hex with:
  1. Output voltage raised toward 14.5 V  (output float 0x46C3->0x46E8)
  2. OVP/VOUT protection threshold raised  (0x0370->0x03B2, as the 20A-CC build)
  3. Constant-current limit set to ~70 A   (case-1 0x0E50 source 0x5511->0x3800)
  4. Current-driven fan: the fan curve's load input is redirected from the
     voltage-weighted power estimate (RAM 0x0896, which collapses in CC mode
     because output voltage is held low) to filtered OUTPUT CURRENT (RAM 0x0E54)
     via a small helper in free flash. The helper returns the same hi/lo flag
     value the stock curve already compares, so no curve thresholds move.

Items 1-3 are the "20A-CC tuning ported to stock, at 70 A + 14.5 V" (the 20A-CC
image is just stock Rev.10 + these 3 constants). Item 4 is the fan fix for the
CC-mode-inherent idle-until-OTP behavior.

The secondary bootloader validates ONLY the low app by an additive byte-sum over
0x0104..0x01FF and 0x0400..0x4FFF (must net to zero; selector at 0x5552). The
high app at 0x6000 is an unconditional fallback. This script keeps the low-app
byte-sum invariant by compensating a trailing padding word, and ASSERTS the sum
is unchanged before writing. The fan helper sits above 0x4FFF (outside the
checksummed range).

Usage:  python3 build_stock_14v5_70a_cc_fan.py
"""
import sys, os

SRC = "original/DSPIC33FJ64GS606 Stock PL30 Rev.10 Sec.hex"
OUT = "tools/DSPIC33FJ64GS606_STOCK_PL30_14V5_70A_CC_fan.hex"

LOW_OFFSET, HIGH_OFFSET = 0x0000, 0x5C00       # low app / high mirror
FAN_CURVE_LOAD_SRC = 0x2DDA                    # stock: MOV 0x896,W1 (load input)
FAN_CURRENT_THRESHOLD = 0x0040                 # 0x0E54 domain; TUNE (free-flash, no cksum cost)

# free-flash window in stock between bootloader and high app
FREE_LO, FREE_HI = 0x5ADC, 0x5FE0
# trailing low-app padding word used to rebalance the checksum (0xFFFFFF -> lower)
COMP_WORD = 0x4FE0

# ---- functional constant patches (low image; high mirror added automatically)
CONST_PATCH = {
    0x39B4: (0x246C33, 0x246E83),   # MOV #0x46C3,W3 -> #0x46E8  (~14.5 V)
    0x3348: (0x203700, 0x203B20),   # MOV #0x370,W0  -> #0x3B2   (OVP threshold)
    0x3446: (0x255110, 0x238000),   # MOV #0x5511,W0 -> #0x3800  (~70 A CC limit)
}


def bsum(v):
    return (v & 0xFF) + ((v >> 8) & 0xFF) + ((v >> 16) & 0xFF)


def parse(path):
    recs = []
    for line in open(path):
        s = line.strip()
        if not s.startswith(":"):
            continue
        bc = int(s[1:3], 16); off = int(s[3:7], 16); rt = int(s[7:9], 16)
        recs.append([bc, off, rt, bytearray.fromhex(s[9:9 + bc * 2])])
    return recs


def emit(bc, off, rt, data):
    body = bytes([bc, (off >> 8) & 0xFF, off & 0xFF, rt]) + bytes(data)
    return ":" + body.hex().upper() + f"{(-sum(body)) & 0xFF:02X}"


def main():
    recs = parse(SRC)
    bytemap = {}
    ela = 0
    recidx = []
    for r in recs:
        bc, off, rt, data = r
        if rt == 4:
            ela = (data[0] << 8) | data[1]
        elif rt == 0:
            base = (ela << 16) + off
            recidx.append((base, r))
            for i, b in enumerate(data):
                bytemap[base + i] = b

    def rd(pw):
        a = pw * 2
        return bytemap.get(a, 0xFF) | (bytemap.get(a + 1, 0xFF) << 8) | (bytemap.get(a + 2, 0xFF) << 16)

    def wr(pw, val):
        a = pw * 2
        nb = [val & 0xFF, (val >> 8) & 0xFF, (val >> 16) & 0xFF, 0x00]
        for i in range(4):
            bytemap[a + i] = nb[i]
            for base, r in recidx:
                if base <= a + i < base + len(r[3]):
                    r[3][a + i - base] = nb[i]
                    break

    def low_app_bytesum():
        s = 0
        for pw in list(range(0x104, 0x200, 2)) + list(range(0x400, 0x5000, 2)):
            s += bsum(rd(pw))
        return s

    baseline = low_app_bytesum()

    # ---- sanity: confirm every source word matches expectation
    for pw, (old, _) in CONST_PATCH.items():
        for img in (LOW_OFFSET, HIGH_OFFSET):
            got = rd(pw + img)
            assert got == old, f"0x{pw+img:04X}: expected {old:06X}, found {got:06X}"
    assert rd(FAN_CURVE_LOAD_SRC) == 0x8044B1, "fan load-src opcode mismatch (low)"
    assert rd(FAN_CURVE_LOAD_SRC + HIGH_OFFSET) == 0x8044B1, "fan load-src mismatch (high)"

    # ---- helper machine code (threshold patched in); placed at chosen base
    def helper_words(base):
        return {
            base + 0x00: 0x8072A1,                               # MOV 0xE54, W1
            base + 0x02: 0x200000 | (FAN_CURRENT_THRESHOLD << 4),# MOV #THRESH, W0
            base + 0x04: 0x508F80,                               # SUB W1, W0, [W15]
            base + 0x06: 0x360002,                               # BRA LEU, +2 (-> CLR)
            base + 0x08: 0x2FFFF1,                               # MOV #0xFFFF, W1
            base + 0x0A: 0x060000,                               # RETURN
            base + 0x0C: 0xEB0080,                               # CLR W1
            base + 0x0E: 0x060000,                               # RETURN
        }

    def rcall(frm, to):
        off = (to - frm - 2) // 2
        return 0x070000 | (off & 0xFFFF)

    # constant-patch byte delta on the low-app checksum
    const_delta = sum(bsum(nw) - bsum(ow) for _, (ow, nw) in CONST_PATCH.items())

    # choose helper base so the low-app net delta is positive and small
    chosen = None
    for base in range(FREE_LO, FREE_HI, 2):
        if any(rd(base + i * 2) != 0xFFFFFF for i in range(8)):
            continue
        fan_delta = bsum(rcall(FAN_CURVE_LOAD_SRC, base)) - bsum(0x8044B1)
        net = const_delta + fan_delta
        if 0 < net <= 0x2FD:
            chosen = (base, net)
            break
    assert chosen, "no free-flash helper base yields a compensatable low-app delta"
    base, net = chosen

    # ---- apply everything
    applied = []
    for pw, (old, new) in CONST_PATCH.items():
        wr(pw, new); applied.append((pw, old, new))
        wr(pw + HIGH_OFFSET, new); applied.append((pw + HIGH_OFFSET, old, new))
    for a, v in helper_words(base).items():
        wr(a, v)
    rc_lo = rcall(FAN_CURVE_LOAD_SRC, base)
    rc_hi = rcall(FAN_CURVE_LOAD_SRC + HIGH_OFFSET, base)
    wr(FAN_CURVE_LOAD_SRC, rc_lo); applied.append((FAN_CURVE_LOAD_SRC, 0x8044B1, rc_lo))
    wr(FAN_CURVE_LOAD_SRC + HIGH_OFFSET, rc_hi)
    applied.append((FAN_CURVE_LOAD_SRC + HIGH_OFFSET, 0x8044B1, rc_hi))

    # ---- compensate low-app checksum: lower COMP_WORD by net
    comp_old = rd(COMP_WORD)
    assert comp_old == 0xFFFFFF, f"comp word 0x{COMP_WORD:04X} not padding ({comp_old:06X})"
    # reduce the byte sum of this word by `net` (spread across its 3 bytes)
    need = net
    b0, b1, b2 = 0xFF, 0xFF, 0xFF
    take = min(need, 0xFF); b0 -= take; need -= take
    take = min(need, 0xFF); b1 -= take; need -= take
    take = min(need, 0xFF); b2 -= take; need -= take
    assert need == 0
    comp_new = b0 | (b1 << 8) | (b2 << 16)
    wr(COMP_WORD, comp_new)

    # ---- ASSERT the low-app checksum byte-sum is invariant
    after = low_app_bytesum()
    assert after == baseline, f"LOW-APP CHECKSUM CHANGED {baseline:#x} -> {after:#x}"

    # ---- write hex
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    with open(OUT, "w") as f:
        for r in recs:
            f.write(emit(*r) + "\n")

    # ---- report
    print("Functional patches (addr: old -> new):")
    for pw, old, new in sorted(applied):
        print(f"  0x{pw:04X}: {old:06X} -> {new:06X}")
    print(f"\nFan helper @0x{base:04X} (free flash, outside checksum):")
    for a, v in sorted(helper_words(base).items()):
        print(f"  0x{a:04X}: {v:06X}")
    print(f"\nFan current threshold (RAM 0x0E54 domain): 0x{FAN_CURRENT_THRESHOLD:04X}  (helper word 0x{base+2:04X}, tune freely — no checksum cost)")
    print(f"Checksum comp word 0x{COMP_WORD:04X}: {comp_old:06X} -> {comp_new:06X}  (net low-app delta was +0x{net:X})")
    print(f"Low-app byte-sum baseline=0x{baseline:06X} after=0x{after:06X}  -> "
          + ("INVARIANT ✓" if after == baseline else "MISMATCH !!!"))
    print("\nOutput:", OUT)


if __name__ == "__main__":
    main()
