#!/usr/bin/env python3
"""
Build a flashable secondary hex for the PL30 PICO firmware with:
  1. Output voltage raised toward 14.5 V  (output-command float 0x46C3->0x46E8)
  2. Load-tracking current-shadow helper so the fan responds to load again
     (replaces the fixed 0x00DE/0x00CC synthetic current with 0x08C4>>6)

It edits the EXISTING modified-PICO binary (not a from-scratch rebuild) and
preserves the bootloader's additive byte-sum checksum over the low app
(0x0104..0x01FF and 0x0400..0x4FFF) by compensating the +0x52 voltage delta
with a -0x52 change to a padding word at 0x4FE0. The high app at 0x6000 is the
unconditional boot fallback and is not checksum-gated.

70 A CC is commanded at runtime from the Pico host (no firmware change).

Usage:  python3 build_pl30_14v5_70a_fanfix_hex.py
"""

import os

SRC = "original_modified_pico/DSPIC33FJ64GS606 PICO PL30 Rev.10 Sec..hex"
OUT = "tools/DSPIC33FJ64GS606_PICO_PL30_14V5_70A_fanfix.hex"

# program-word address -> new 24-bit instruction word (None entries unused)
PATCH = {}

# ---- 1. output-command float high word, both images: MOV #0x46C3 -> #0x46E8
PATCH[0x39B4] = 0x246E83          # low image  (was 0x246C33)
PATCH[0x95B4] = 0x246E83          # high image (was 0x246C33)

# ---- 1b. low-app checksum compensation: 0xFFFFFF -> 0xFFFFAD  (-0x52 byte sum)
PATCH[0x4FE0] = 0xFFFFAD          # padding word inside the checksummed range

# ---- 2. load-tracking current-shadow helper (low image at 0x5B30)
FAN_LOW = {
    0x5B30: 0xABE8C9,  # BTST 0x8C9,#7         (unchanged)
    0x5B32: 0x3A0006,  # BRA NZ, 0x5B40        (retarget enabled branch)
    0x5B34: 0x203A31,  # MOV #0x3A3, W1        disabled: idle shadow
    0x5B36: 0x887551,  # MOV W1, 0xEAA
    0x5B38: 0x200002,  # MOV #0x0, W2
    0x5B3A: 0x887562,  # MOV W2, 0xEAC
    0x5B3C: 0x060000,  # RETURN
    0x5B3E: 0x000000,  # NOP (pad)
    0x5B40: 0x804621,  # MOV 0x8C4, W1         enabled: shadow ~ commanded CC
    0x5B42: 0xDE08C6,  # LSR W1, #6, W1
    0x5B44: 0x887551,  # MOV W1, 0xEAA
    0x5B46: 0x804622,  # MOV 0x8C4, W2
    0x5B48: 0xDE1146,  # LSR W2, #6, W2
    0x5B4A: 0x887562,  # MOV W2, 0xEAC
    0x5B4C: 0x060000,  # RETURN
}
# high image mirror at 0xA270 (identical words; RAM operands are the same)
FAN_HIGH = {a - 0x5B30 + 0xA270: v for a, v in FAN_LOW.items()}

PATCH.update(FAN_LOW)
PATCH.update(FAN_HIGH)


def word_to_bytes(v):
    """24-bit program word -> 4 hex bytes (LE 3 bytes + 0x00 phantom)."""
    return bytes([v & 0xFF, (v >> 8) & 0xFF, (v >> 16) & 0xFF, 0x00])


def parse_records(path):
    recs = []
    for line in open(path):
        line = line.strip()
        if not line.startswith(":"):
            continue
        bc = int(line[1:3], 16)
        off = int(line[3:7], 16)
        rt = int(line[7:9], 16)
        data = bytearray.fromhex(line[9:9 + bc * 2])
        recs.append([bc, off, rt, data])
    return recs


def emit_record(bc, off, rt, data):
    body = bytes([bc, (off >> 8) & 0xFF, off & 0xFF, rt]) + bytes(data)
    chk = (-sum(body)) & 0xFF
    return ":" + body.hex().upper() + f"{chk:02X}"


def main():
    recs = parse_records(SRC)

    # build flat byte map with absolute (ELA-extended) addresses
    bytemap = {}
    ela = 0
    rec_index = []   # (abs_base, rec) for type-0 records
    for rec in recs:
        bc, off, rt, data = rec
        if rt == 4:
            ela = (data[0] << 8) | data[1]
        elif rt == 0:
            base = (ela << 16) + off
            rec_index.append((base, rec))
            for i, b in enumerate(data):
                bytemap[base + i] = b

    def byte_sum_low_app(bm):
        """Sum low+mid+high bytes over 0x104..0x1FF and 0x400..0x4FFF."""
        s = 0
        for pw in list(range(0x104, 0x200, 2)) + list(range(0x400, 0x5000, 2)):
            ba = pw * 2
            s += bm.get(ba, 0xFF) + bm.get(ba + 1, 0xFF) + bm.get(ba + 2, 0xFF)
        return s

    before = byte_sum_low_app(bytemap)

    # apply patches into the per-record data buffers (in place)
    applied = {}
    for pw, newv in PATCH.items():
        ba = pw * 2
        nb = word_to_bytes(newv)
        oldb = bytes(bytemap.get(ba + i, 0xFF) for i in range(4))
        applied[pw] = (int.from_bytes(oldb[:3], "little"), newv)
        for i in range(4):
            bytemap[ba + i] = nb[i]
            # write into the owning record
            for base, rec in rec_index:
                if base <= ba + i < base + len(rec[3]):
                    rec[3][ba + i - base] = nb[i]
                    break

    after = byte_sum_low_app(bytemap)

    # re-emit
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    with open(OUT, "w") as f:
        for rec in recs:
            f.write(emit_record(*rec) + "\n")

    # report
    print("Patched program words (addr: old -> new):")
    for pw in sorted(applied):
        old, new = applied[pw]
        print(f"  0x{pw:04X}: {old:06X} -> {new:06X}")
    print()
    print(f"Low-app checksum byte-sum  before: 0x{before:06X}")
    print(f"Low-app checksum byte-sum  after : 0x{after:06X}")
    print("CHECKSUM PRESERVED ✓" if before == after
          else "!!! CHECKSUM CHANGED — low app would fail boot validation !!!")
    print()
    print("Output:", OUT)


if __name__ == "__main__":
    main()
