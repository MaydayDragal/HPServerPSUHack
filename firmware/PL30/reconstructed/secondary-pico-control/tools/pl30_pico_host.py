"""
pl30_pico_host.py  —  MicroPython controller for the PL30 PICO-firmware PSU.

Target: Raspberry Pi Pico running MicroPython.
PSU:    HP HSTNS-PL30 1200W with secondary-side PICO firmware (Rev.10).

Wiring
------
  PSU SMBus SDA  →  Pico GP4
  PSU SMBus SCL  →  Pico GP5
  PSU standby 5V →  Pico VSYS
  PSU GND        →  Pico GND

Usage (REPL)
------------
  from pl30_pico_host import PL30
  psu = PL30()            # auto-scans I2C for the PSU
  psu.set_current(10.0)   # 10 A CC limit
  psu.enable()            # output on, CC mode
  psu.disable()           # output off
  psu.set_cv()            # switch back to voltage-control mode

Quick interactive demo (buttons on Pico Display Pack 2 or GP buttons):
  psu.run_button_demo()

Protocol reference
------------------
  Write transaction (5 bytes after device-address byte):
    [WIRE_REG] [PAYLOAD_LO] [PAYLOAD_HI] [CHECKSUM]
  where WIRE_REG = logical_reg << 1
        CHECKSUM = (0 - WIRE_REG - PAYLOAD_LO - PAYLOAD_HI) & 0xFF

  Logical registers added by the PICO patch:
    0x1F  (wire 0x3E)  CC setpoint in counts = amps * 265.0
    0x21  (wire 0x42)  status/control: bit15 = output-enable, bit0 = CC-mode
"""

from machine import I2C, Pin
import time

# ── PSU protocol constants ──────────────────────────────────────────────────

_LOGICAL_REG_CC      = 0x1F
_LOGICAL_REG_STATUS  = 0x21

_WIRE_REG_CC         = _LOGICAL_REG_CC     << 1   # 0x3E
_WIRE_REG_STATUS     = _LOGICAL_REG_STATUS << 1   # 0x42

_STATUS_OUTPUT_ENABLE = 0x8000
_STATUS_CC_ENABLE     = 0x0001

_COUNTS_PER_AMP  = 265.0
_CC_MIN_COUNTS   = 0x0500   # 1280 counts ≈ 4.83 A  (firmware floor)
_CC_MIN_AMPS     = _CC_MIN_COUNTS / _COUNTS_PER_AMP

# I2C bus parameters
_I2C_FREQ_HZ     = 100_000  # 100 kHz; drop to 50_000 if reliability issues occur
_SDA_PIN         = 4
_SCL_PIN         = 5

# Addresses to try when scanning (set DEVICE_ADDR to skip scan)
DEVICE_ADDR = None   # set to e.g. 0x58 to skip auto-detection


class PL30:
    """Controller for the PL30 secondary PICO firmware over I2C."""

    def __init__(self, addr=None, sda=_SDA_PIN, scl=_SCL_PIN, freq=_I2C_FREQ_HZ):
        self._i2c = I2C(0, sda=Pin(sda), scl=Pin(scl), freq=freq)
        if addr is not None:
            self._addr = addr
        elif DEVICE_ADDR is not None:
            self._addr = DEVICE_ADDR
        else:
            self._addr = self._scan()
        self._cc_counts  = _CC_MIN_COUNTS
        self._status_val = 0x0000

    # ── I2C discovery ───────────────────────────────────────────────────────

    def _scan(self):
        """Return the first I2C address that responds, or raise."""
        found = self._i2c.scan()
        if not found:
            raise RuntimeError("No I2C device found.  Check wiring and PSU standby power.")
        if len(found) > 1:
            print(f"Multiple devices found: {[hex(a) for a in found]}")
            print(f"Using first: {hex(found[0])}")
        else:
            print(f"PSU found at I2C address {hex(found[0])}")
        return found[0]

    @property
    def address(self):
        return self._addr

    # ── Wire-protocol helpers ────────────────────────────────────────────────

    @staticmethod
    def _checksum(wire_reg, lo, hi):
        return (-(wire_reg + lo + hi)) & 0xFF

    def _write_register(self, wire_reg, value_u16):
        """Send a 5-byte write frame to the PSU."""
        lo = value_u16 & 0xFF
        hi = (value_u16 >> 8) & 0xFF
        cs = self._checksum(wire_reg, lo, hi)
        buf = bytes([wire_reg, lo, hi, cs])
        self._i2c.writeto(self._addr, buf)

    # ── CC setpoint ──────────────────────────────────────────────────────────

    def set_current(self, amps):
        """
        Set constant-current limit.  amps must be >= 4.83 (firmware floor).
        The firmware silently clamps values below the floor to 4.83 A.
        """
        counts = int(round(amps * _COUNTS_PER_AMP))
        counts = max(counts, _CC_MIN_COUNTS)
        if counts > 0xFFFF:
            raise ValueError(f"Current {amps:.1f} A exceeds 16-bit count range")
        self._cc_counts = counts
        self._write_register(_WIRE_REG_CC, counts)

    @property
    def current_setpoint_amps(self):
        return self._cc_counts / _COUNTS_PER_AMP

    @property
    def current_setpoint_counts(self):
        return self._cc_counts

    # ── Output enable / mode control ─────────────────────────────────────────

    def _write_status(self, value):
        self._status_val = value & 0xFFFF
        self._write_register(_WIRE_REG_STATUS, self._status_val)

    def enable(self, cc=True):
        """
        Enable PSU output.
        cc=True  → constant-current mode (reg 0x1F setpoint active).
        cc=False → constant-voltage mode (stock voltage control loop).
        Write set_current() before calling enable(cc=True).
        """
        bits = _STATUS_OUTPUT_ENABLE
        if cc:
            bits |= _STATUS_CC_ENABLE
        self._write_status(bits)

    def disable(self):
        """Turn off the main output.  PSU returns to standby."""
        self._write_status(0x0000)

    def set_cc(self):
        """Switch to CC mode while output is already enabled."""
        self._write_status(self._status_val | _STATUS_CC_ENABLE)

    def set_cv(self):
        """Switch to CV mode while output is already enabled."""
        self._write_status(self._status_val & ~_STATUS_CC_ENABLE)

    @property
    def output_enabled(self):
        return bool(self._status_val & _STATUS_OUTPUT_ENABLE)

    @property
    def cc_mode(self):
        return bool(self._status_val & _STATUS_CC_ENABLE)

    # ── Convenience helpers ──────────────────────────────────────────────────

    def ramp_current(self, target_amps, step_amps=1.0, delay_s=0.5):
        """Gradually increase CC limit from current setpoint to target."""
        current = self.current_setpoint_amps
        if target_amps <= current:
            self.set_current(target_amps)
            return
        amps = current
        while amps < target_amps:
            amps = min(amps + step_amps, target_amps)
            self.set_current(amps)
            print(f"  CC → {amps:.1f} A ({self._cc_counts} counts)")
            time.sleep(delay_s)

    def status_str(self):
        """Return a human-readable status line."""
        out = "ON" if self.output_enabled else "OFF"
        mode = "CC" if self.cc_mode else "CV"
        return f"output={out}  mode={mode}  setpoint={self.current_setpoint_amps:.2f}A ({self._cc_counts} counts)  addr={hex(self._addr)}"

    # ── Interactive button demo ──────────────────────────────────────────────

    def run_button_demo(self,
                        btn_enable=15,   # A  on Pico Display Pack 2
                        btn_mode=14,     # X
                        btn_up=13,       # B
                        btn_down=12,     # Y
                        step_amps=1.0,
                        min_amps=5.0,
                        max_amps=20.0):
        """
        Simple button-driven demo matching the original zpsu button layout:
          btn_enable (A): toggle output on/off
          btn_mode   (X): toggle CC/CV
          btn_up     (B): increase CC setpoint by step_amps
          btn_down   (Y): decrease CC setpoint by step_amps

        Runs until interrupted with CTRL+C.
        Adjust pin numbers for your board.
        """
        btns = {
            'enable': Pin(btn_enable, Pin.IN, Pin.PULL_UP),
            'mode':   Pin(btn_mode,   Pin.IN, Pin.PULL_UP),
            'up':     Pin(btn_up,     Pin.IN, Pin.PULL_UP),
            'down':   Pin(btn_down,   Pin.IN, Pin.PULL_UP),
        }
        last = {k: 1 for k in btns}
        self.set_current(min_amps)
        print("Button demo started.  CTRL+C to exit.")
        print(self.status_str())
        try:
            while True:
                for name, pin in btns.items():
                    val = pin.value()
                    if last[name] == 1 and val == 0:  # falling edge = press
                        if name == 'enable':
                            if self.output_enabled:
                                self.disable()
                            else:
                                self.enable(cc=self.cc_mode)
                        elif name == 'mode':
                            if self.cc_mode:
                                self.set_cv()
                            else:
                                self.set_cc()
                        elif name == 'up':
                            new_a = min(self.current_setpoint_amps + step_amps, max_amps)
                            self.set_current(new_a)
                        elif name == 'down':
                            new_a = max(self.current_setpoint_amps - step_amps, min_amps)
                            self.set_current(new_a)
                        print(self.status_str())
                    last[name] = val
                time.sleep_ms(20)
        except KeyboardInterrupt:
            self.disable()
            print("Output disabled.  Exiting.")


# ── Quick demo when run directly ─────────────────────────────────────────────

if __name__ == "__main__":
    psu = PL30()
    print("Setting 5 A CC limit...")
    psu.set_current(5.0)
    print("Enabling output in CC mode...")
    psu.enable(cc=True)
    print(psu.status_str())
    print("Waiting 5 s...")
    time.sleep(5)
    print("Disabling output.")
    psu.disable()
    print("Done.")
