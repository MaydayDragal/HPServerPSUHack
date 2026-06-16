"""
pl30_fan_supervisor.py  —  External load+temperature fan control (fallback).

Use this ONLY if the firmware-side fan fix in analysis/test_14v5_70a_setup.md
does not restore load response (i.e. the fan's load input keys off a channel
the current-substitute helper does not feed). This moves the fan curve onto the
Pico: it computes a fan target from commanded load + measured temperature and
writes it to the firmware.

FIRMWARE REGISTERS THIS NEEDS (must be added to the PICO patch):

  Write reg 0x23 (wire 0x46): fan target.
      Handler writes the received 16-bit word to RAM 0x0838 (fan setpoint) and
      forces the cooling-demand flag set (BSET 0x85E,#7) so the stock fan curve
      scheduler at 0x2F72 applies it. Without the forced flag, the scheduler
      won't call the fan curve and the target is ignored.

  Read reg 0x40 (wire 0x80): primary temperature -> RAM 0x0E2C (ADCBUF10).
  Read reg 0x41 (wire 0x82): secondary temperature -> RAM 0x0E2A (ADCBUF11).
      Map these into the stock I2C1 read-register path. If you have not added
      temperature reads yet, run this supervisor in load-only mode (it still
      ramps the fan with commanded current); temperature is then a no-op until
      the read registers exist.

The fan target domain is the stock fan setpoint at 0x0838, clamped by firmware
to <= 0x4650 (18000). Higher target = faster fan demand. The mapping from
"percent" to that domain is unit-specific; tune FAN_TARGET_FULL below on a meter.
"""

import time
from pl30_pico_host import PL30

# Wire registers (logical << 1)
_WIRE_FAN_TARGET = 0x23 << 1   # 0x46
_WIRE_RD_TEMP_PRI = 0x40 << 1  # 0x80
_WIRE_RD_TEMP_SEC = 0x41 << 1  # 0x82

# Fan-target domain (RAM 0x0838), firmware-clamped to <= 0x4650.
FAN_TARGET_IDLE = 0x0800       # minimum / idle demand   (tune on a meter)
FAN_TARGET_FULL = 0x4650       # full-speed demand (firmware ceiling)

# Curve thresholds
LOAD_FULL_AMPS  = 70.0         # commanded current that maps to full fan
LOAD_MIN_AMPS   = 10.0         # below this, load contributes nothing
TEMP_MIN_C      = 40.0         # below this, temperature contributes nothing
TEMP_FULL_C     = 75.0         # at/above this, temperature alone => full fan

# Temperature ADC -> degrees C. Unit-specific; calibrate against a probe.
# Placeholder linear fit: degC = adc_counts * TEMP_SLOPE + TEMP_OFFSET.
TEMP_SLOPE  = 0.25
TEMP_OFFSET = 0.0


def _clamp(v, lo, hi):
    return lo if v < lo else hi if v > hi else v


class PL30FanSupervisor:
    def __init__(self, psu=None, read_temps=False):
        self._psu = psu or PL30()
        self._read_temps = read_temps

    # ── telemetry ────────────────────────────────────────────────────────────

    def _read_reg_u16(self, wire_reg):
        """Read a 16-bit value from a firmware read register (2 bytes, LE)."""
        data = self._psu._i2c.readfrom_mem(self._psu.address, wire_reg, 2)
        return data[0] | (data[1] << 8)

    def temperatures_c(self):
        """(primary_C, secondary_C) or (None, None) if temp reads unavailable."""
        if not self._read_temps:
            return (None, None)
        try:
            pri = self._read_reg_u16(_WIRE_RD_TEMP_PRI)
            sec = self._read_reg_u16(_WIRE_RD_TEMP_SEC)
            return (pri * TEMP_SLOPE + TEMP_OFFSET,
                    sec * TEMP_SLOPE + TEMP_OFFSET)
        except OSError:
            return (None, None)

    # ── curve ─────────────────────────────────────────────────────────────────

    @staticmethod
    def _frac_load(commanded_amps):
        if commanded_amps <= LOAD_MIN_AMPS:
            return 0.0
        if commanded_amps >= LOAD_FULL_AMPS:
            return 1.0
        return (commanded_amps - LOAD_MIN_AMPS) / (LOAD_FULL_AMPS - LOAD_MIN_AMPS)

    @staticmethod
    def _frac_temp(temp_c):
        if temp_c is None or temp_c <= TEMP_MIN_C:
            return 0.0
        if temp_c >= TEMP_FULL_C:
            return 1.0
        return (temp_c - TEMP_MIN_C) / (TEMP_FULL_C - TEMP_MIN_C)

    def compute_target(self, commanded_amps, temp_c):
        """Fan target in the 0x0838 domain. Load and temperature, take the max."""
        frac = max(self._frac_load(commanded_amps), self._frac_temp(temp_c))
        span = FAN_TARGET_FULL - FAN_TARGET_IDLE
        return int(_clamp(FAN_TARGET_IDLE + frac * span,
                          FAN_TARGET_IDLE, FAN_TARGET_FULL))

    # ── output ──────────────────────────────────────────────────────────────

    def set_fan_target(self, target):
        self._psu._write_register(_WIRE_FAN_TARGET, target & 0xFFFF)

    # ── loop ──────────────────────────────────────────────────────────────────

    def run(self, period_s=1.0, dry_run=False):
        """
        Continuously drive the fan from commanded current + temperature.
        dry_run=True prints the computed target without writing (for tuning).
        """
        print("Fan supervisor running. CTRL+C to stop.")
        try:
            while True:
                amps = self._psu.current_setpoint_amps
                tpri, tsec = self.temperatures_c()
                temp = max(t for t in (tpri, tsec, -999) if t is not None)
                temp = None if temp == -999 else temp
                target = self.compute_target(amps, temp)
                t_str = f"{temp:.1f}C" if temp is not None else "n/a"
                print(f"load={amps:4.0f}A temp={t_str:>6} -> fan_target=0x{target:04X}")
                if not dry_run:
                    self.set_fan_target(target)
                time.sleep(period_s)
        except KeyboardInterrupt:
            print("Fan supervisor stopped.")


if __name__ == "__main__":
    # Dry run by default so it is safe to import/run without the firmware regs.
    PL30FanSupervisor(read_temps=False).run(dry_run=True)
