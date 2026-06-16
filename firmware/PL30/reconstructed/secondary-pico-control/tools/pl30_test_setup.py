"""
pl30_test_setup.py  —  Bench test: 14.5 V / 70 A CC on the PL30 (PICO firmware).

Target: Raspberry Pi Pico (MicroPython), talking to a PL30 running the
PICO-control secondary firmware.  Builds on pl30_pico_host.PL30.

WHAT THIS SCRIPT DOES (host side):
  - Commands a 70 A constant-current limit over I2C (reg 0x1F).
  - Enables the output in CC mode (reg 0x21).
  - Ramps the current up in steps so you can watch the supply on a meter.

WHAT THIS SCRIPT DOES NOT DO:
  - It does NOT set 14.5 V.  Output voltage is the firmware CV reference and
    is not exposed as an I2C register in the stock PICO patch.  You set 14.5 V
    by flashing the output-command float change described in
    analysis/test_14v5_70a_setup.md.  Verify with a meter; trim the constant.
  - It does NOT control the fan.  The fan PWM (SDC4) is driven by the dsPIC.
    Load+temperature fan response is restored by the firmware helper change in
    the same doc.  An optional external supervisor is in pl30_fan_supervisor.py.

!!!  SAFETY  !!!
  14.5 V * 70 A ~= 1015 W, near the 1200 W rating.  Test into an appropriate
  electronic/resistive load or a battery with its own protection, on a
  current-limited bench source first.  Confirm the CC scale (counts/A) on a
  meter at LOW current before trusting a 70 A command — the 265 counts/A figure
  is the zpsu convention and may not map 1:1 to this unit's internal limit.
"""

import time
from pl30_pico_host import PL30

# ── Target operating point ───────────────────────────────────────────────────
TARGET_AMPS      = 70.0     # constant-current limit
START_AMPS       = 5.0      # first commanded current (safe, above 4.83 A floor)
RAMP_STEP_AMPS   = 5.0      # increment per step while ramping up
RAMP_DELAY_S     = 1.0      # dwell at each step

# 70 A * 265 counts/A = 18550 = 0x4876.  This clears the firmware CC floor
# (0x0500 = 1280 counts) and should sit under the mode-table ceiling at RAM
# 0x0E50, but confirm the unit actually delivers ~70 A — see SAFETY note.


def main():
    psu = PL30()                      # auto-scans the I2C bus for the PSU
    print("PSU @", hex(psu.address))

    print(f"Setting starting CC limit {START_AMPS:.1f} A ...")
    psu.set_current(START_AMPS)

    print("Enabling output in CC mode ...")
    psu.enable(cc=True)
    print(psu.status_str())

    print(f"Ramping CC limit {START_AMPS:.0f} A -> {TARGET_AMPS:.0f} A ...")
    psu.ramp_current(TARGET_AMPS, step_amps=RAMP_STEP_AMPS, delay_s=RAMP_DELAY_S)

    print("At target.")
    print(psu.status_str())
    print("Measure output voltage (expect ~14.5 V if the firmware float is set)")
    print("and output current.  CTRL+C then psu.disable() to stop.")


def stop():
    """Convenience: disable output from the REPL."""
    PL30().disable()
    print("Output disabled.")


if __name__ == "__main__":
    try:
        main()
    except KeyboardInterrupt:
        stop()
