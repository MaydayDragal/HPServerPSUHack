# PL30 Pico-Control Quick-Start

Target: HP HSTNS-PL30 1200W, secondary-side dsPIC33FJ64GS606, PICO Rev.10 firmware.

## What this modification does

The stock secondary firmware already runs an I2C1 slave for HP management-bus commands.
The PICO patch adds two extra writable registers to that slave:

| Logical reg | Wire byte | Meaning |
|---|---|---|
| `0x1F` | `0x3E` | Constant-current setpoint in ADC counts.  `counts = amps × 265.0`. |
| `0x21` | `0x42` | Status/control word.  `bit15` = output enable, `bit0` = CC mode. |

No new hardware is needed inside the PSU.  A Raspberry Pi Pico (or RPi, or any I2C master) connects to the PSU's existing I2C pins from outside.

The PSU runs in constant-voltage (CV) mode by default.  Writing `0x8001` to reg `0x21` switches to constant-current and applies the limit last written to reg `0x1F`.

---

## Physical connections

The external I2C host interface (I2C1 slave) exits the PSU on two pins of the backplane/edge connector.  Connect your Pico as follows:

| PSU edge connector | Signal | Pico GPIO |
|---|---|---|
| SMBus / I2C SDA | SDA | GP4 |
| SMBus / I2C SCL | SCL | GP5 |
| Standby 5 V | VSYS power | VSYS |
| GND | GND | GND |

The standby 5 V rail is present whenever the PSU has mains input, even when the main output is off.  It powers the Pico directly.  Do not connect the Pico 3.3V regulator output back to the PSU board.

> The I2C1 slave on the dsPIC33FJ64GS606 uses 3.3 V logic levels.  Pico GPIO is 3.3 V — a direct connection is correct.  Do not add a level shifter.

### Finding the I2C device address

The firmware derives the I2C1 slave address at run time:

```
addr = (board_id_eeprom_byte & 0x7F) XOR (PORTD[2:0])
```

The address is not fixed; it depends on the AT24C01D board-ID EEPROM contents and three GPIO pins that vary per unit.  **Scan the bus** on first use:

```python
from machine import I2C, Pin
i2c = I2C(0, sda=Pin(4), scl=Pin(5), freq=100_000)
found = i2c.scan()
print([hex(a) for a in found])   # e.g. ['0x58']
```

Typical addresses seen in the field: `0x40`, `0x58`.  Use the found address as `DEVICE_ADDR` in the examples below.

---

## I2C write-transaction wire format

Every write to the PSU follows this 5-byte structure (excluding the I2C address byte):

```
[START]
[DEVICE_ADDR << 1]        ; 7-bit address + write bit
[WIRE_REG]                ; logical_reg << 1
[PAYLOAD_LO]              ; bits 7:0 of 16-bit value
[PAYLOAD_HI]              ; bits 15:8 of 16-bit value
[CHECKSUM]                ; two's-complement: (0 - WIRE_REG - PAYLOAD_LO - PAYLOAD_HI) & 0xFF
[STOP]
```

The checksum byte makes the 8-bit sum of bytes 2–5 equal zero modulo 256.

Example — set 10.0 A constant current:
```
counts = round(10.0 * 265.0) = 2650 = 0x0A5A
WIRE_REG  = 0x3E  (logical 0x1F << 1)
PAYLOAD_LO = 0x5A
PAYLOAD_HI = 0x0A
CHECKSUM   = (0 - 0x3E - 0x5A - 0x0A) & 0xFF = (0 - 0xA2) & 0xFF = 0x5E
Transaction: [ADDR+W] 3E 5A 0A 5E
```

Example — enable output in CC mode:
```
value = 0x8001  (bit15 output-enable, bit0 CC-mode)
WIRE_REG  = 0x42  (logical 0x21 << 1)
PAYLOAD_LO = 0x01
PAYLOAD_HI = 0x80
CHECKSUM   = (0 - 0x42 - 0x01 - 0x80) & 0xFF = (0 - 0xC3) & 0xFF = 0x3D
Transaction: [ADDR+W] 42 01 80 3D
```

Example — disable output:
```
value = 0x0000
WIRE_REG  = 0x42
PAYLOAD_LO = 0x00, PAYLOAD_HI = 0x00
CHECKSUM   = 0x00 - 0x42 = 0xBE
Transaction: [ADDR+W] 42 00 00 BE
```

---

## Constant-current range

The CC limit is bounded by firmware before being applied:

| Bound | Counts | Amps (÷ 265.0) |
|---|---|---|
| Minimum (hard floor in patch) | `0x0500` = 1280 | ≈ 4.83 A |
| Maximum (mode-table ceiling at RAM 0x0E50) | runtime value | set by existing firmware |

The mode-table ceiling is written each cycle by the stock control loop.  For a 1200 W unit at 12 V the practical maximum is around 100 A, but the firmware may set a lower ceiling depending on operating state and calibration.

Values below the 4.83 A floor are silently clamped up to 1280 counts.
Values above the mode-table ceiling are silently clamped down.

---

## Status/control word (reg 0x21, wire 0x42)

| Bit | Name | Meaning when set |
|---|---|---|
| 15 | `OUTPUT_ENABLE` | Request main output on.  Cleared = output off (PSU goes to standby). |
| 0  | `CC_ENABLE`     | Apply the constant-current limit from reg `0x1F`.  Cleared = CV (voltage-control) mode. |

Write order matters: write reg `0x1F` with the desired current **before** setting `CC_ENABLE`.  The CC limit is applied each control-loop iteration only when bit 0 is set.

---

## Synthetic current-ADC values

The patch replaces the raw `ADCBUF6`/`ADCBUF7` copy with fixed shadow words that keep the stock current-monitor path in a valid state:

| Output state | Shadow A (RAM 0x0EAA) | Shadow B (RAM 0x0EAC) | A in counts/A scale |
|---|---|---|---|
| Disabled (`OUTPUT_ENABLE` = 0) | `0x03A3` = 931 | `0x0000` = 0 | ≈ 3.5 A (high-idle bias) |
| Enabled (`OUTPUT_ENABLE` = 1)  | `0x00DE` = 222 | `0x00CC` = 204 | ≈ 0.84 A (low-on bias)  |

These are not real current readings.  They bias the existing protection logic so it does not trip on apparent zero-current or overcurrent faults while the external CC loop controls the actual output.

---

## OVP sense scaling

The PICO image (like NoHWMod) applies a Q15 scale factor to the raw output-voltage ADC sample before the OVP comparator:

```
effective_adc = (ADCBUF2 × 0x6AC2) >> 15   ≈ ADCBUF2 × 0.8340
```

This compensates for a resistor-divider modification on the OVP feedback path.  If you are using the PSU unmodified (stock resistors), the OVP threshold will appear lower than intended — the PSU will trip at about 83% of the nominal OVP voltage.  Remove the hardware OVP resistor mod or adjust the OVP threshold register if you need the full trip point with an unmodified board.

---

## Step-by-step first use

1. Connect Pico to PSU as described above.
2. Apply mains to the PSU.  Standby 5 V appears; Pico boots.
3. Scan the I2C bus and note the address.
4. Write a safe CC limit first (e.g. 5 A → `counts = 1325 = 0x052D`).
5. Enable output in CC mode: write `0x8001` to reg `0x21`.
6. Measure output voltage and current with a meter.
7. Increase CC setpoint gradually.

See `tools/pl30_pico_host.py` for a MicroPython implementation of all of the above.

---

## Key addresses at a glance

| Symbol | RAM address | Description |
|---|---|---|
| `SEC_PICO_CC_COMMAND_COUNTS` | `0x08C4` | CC setpoint written by reg `0x1F` handler |
| `SEC_PICO_STATUS_COMMAND`    | `0x08C8` | Status/control written by reg `0x21` handler |
| `SEC_CC_MODE_E50_LIMIT`      | `0x0E50` | Mode-table current ceiling clamped by CC helper |
| `SEC_ADC_VBUS_FILTERED`      | `0x0A02` | Filtered output-voltage ADC count |
| `SEC_ADC_CURRENT_FILTER_A`   | `0x0E54` | Filtered current ADC (startup default `0x03FF`) |
| `SEC_APP_STATE`              | `0x0E1E` | State machine: 1=idle, 2=pending, 3=run, 4=shutdown |
