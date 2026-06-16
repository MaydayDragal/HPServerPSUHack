# GitHub protocol context used during reconstruction

- HPServerPSUHack documents the PL30 output/OVP voltage modification and the PICO wiring:
  - Pico VSYS to standby 5 V, GND to GND, GP4/SDA to PSU pin 31, GP5/SCL to PSU pin 32.
  - Buttons: A enable/disable PSU, X CV/CC switch, B/Y increase/decrease CC setting.
  - The OVP helper scales ADCBUF2 by Q15 coefficient 0x6AC2, equivalent to approximately 0.834.
- zpsu uses the dsPIC I2C register protocol:
  - logical register 0x1F: constant-current setpoint, encoded as amps * 265.0.
  - logical register 0x21: status/control word; bit15 is output-enable, bit0 is CC mode.
  - writes use wire register = logical register << 1 and a one-byte two's-complement checksum.

URLs:
- https://github.com/darwinbeing/HPServerPSUHack
- https://github.com/darwinbeing/zpsu
