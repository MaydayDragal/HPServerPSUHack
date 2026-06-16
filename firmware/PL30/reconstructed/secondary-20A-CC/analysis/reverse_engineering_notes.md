# PL30 Rev.06 secondary-side dsPIC33FJ64GS606 reverse-engineering notes

## Scope

This package is a first-pass source-level reconstruction of the secondary-side IC firmware.  It is intended to make the control flow, startup defaults, safety interlocks, communication protocol, and calibration paths editable inside MPLAB/XC16.  It is not a bit-identical recovery of the original source.

## Memory layout

The reset vector at `0x0000` jumps to `0x5000`, so reset enters the boot/update selector rather than the application directly.

Recovered non-blank program ranges:

| Range | Role |
|---|---|
| `0x0000..0x00FE` | Normal IVT/dispatch table; most entries point into high image. |
| `0x0104..0x01FE` | Alternate IVT/dispatch table; entries point into low image. |
| `0x0400..0x4500` | Low application copy. |
| `0x450C..0x460C` | Low app trap handlers and C-startup data table. |
| `0x4FF8..0x5ADA` | Bootloader/update selector. |
| `0x6000..0xA100` | High application copy, same logic as low image with relocated absolute calls. |
| `0xA10C..0xA20C` | High app trap handlers and data table. |

The application entry points are `0x3C44` in the low image and `0x9844` in the high image.  The two app copies are functionally duplicated; the differences are relocation-sensitive absolute calls/GOTOs and data-table addresses.

## Configuration bits

The stock secondary-side configuration uses FRC oscillator startup, primary oscillator disabled, clock switching enabled with fail-safe clock monitor disabled, software-controlled watchdog, 128 ms power-up timer, PGD2/PGC2 debug channel, JTAG disabled, and comparator hysteresis configured to 45 mV on both even and odd comparator groups.

## Application startup

Application startup initializes:

1. PLL/clock/auxiliary PWM clock: `PLLFBD=0x29`, `ACLKCON=0xA740`, `PTCON2=1`.
2. GPIO: LAT/TRIS setup for ports B/C/D/E/F/G, including board-mode-dependent TRISC/PWM3 behavior.
3. Timers: `PR1=0x9C40`, `PR2=0x1F40`, Timer1 interrupt enable.
4. UART1 protocol: `U1BRG=0x1A`, protocol state variables around `0x09B2..0x09D2`, retry defaults of 5.
5. PWM3/PWM4/PWM6: PWM3 gate path, PWM4 soft-start parameters, PWM6 default duty `PDC6=0x445C`.
6. ADC: `ADPCFG=0xF000`, ADC pair interrupts enabled through IEC7, ADC enabled after calibration load.
7. External config/calibration: routines `0x14EC` and `0x167A` move I2C2/EEPROM-backed values into `0x0D50..0x0D7B`.

## Main loop

The low-image main loop at `0x3C7A` runs a service sequence rather than a single blocking state machine:

```c
for (;;) {
    select_i2c1_address();
    uart1_housekeeping();
    comm_fault_latch();
    evaluate_output_faults();
    pwm6_control_update();
    refresh_calibration_if_dirty();
    fault_counter_a_service();
    fault_counter_b_service();
    pwm_ramp_update();
    state_gate_update();
    status_latch_update();
    state_machine();
    i2c2_transaction_service();

    switch (APP_STATE) {
    case 1: idle/await-command; break;
    case 2: pending-run; break;
    case 3: ramp/run; break;
    case 4: shutdown, disable interrupts, jump to 0x5000; break;
    }
}
```

## Interrupts and fast loops

The important ISR-level logic is:

- Timer1/housekeeping ISR increments `0x0A30`, UART timeouts, address-selection counters, and multiple debounce/fault counters.
- UART1 RX ISR parses `0x05` query frames and `0x50` command/value frames, writing a contiguous register window beginning around `0x0986`.
- I2C1 slave ISR implements the secondary-side command/status protocol used elsewhere in the PL30 system.
- I2C2 master ISR and helper routines service external configuration/calibration transactions.
- ADC pair ISRs copy `ADCBUF` results to RAM, low-pass filter selected channels, set fault/permissive flags, and update PWM6 duty through the active regulation path.

## Boot/update selector

The bootloader at `0x5000` initializes a safe subset of clock/GPIO/timer/PWM hardware, reads a flash marker around `0x5C00`, validates the low application image, and either jumps to `0x0400` or falls back to the high application at `0x6000`.  Validation uses metadata copied from the end of the low image, a required marker value `0x003C`, and a checksum over the low IVT/application region.  Boot-mode I2C services can receive flash chunks and request NVM erase/program operations.

## Safety-critical edit points

- `SEC_PDC6` (`PDC6`) is the most obvious secondary-side active PWM duty output.
- `SEC_PDC3`, `SEC_PDC4`, and `SEC_PDC6` are all forced to zero during shutdown/boot handoff.
- `SEC_POWER_FLAGS` and `SEC_PWM_ENABLE_FLAGS` are central interlocks.  Do not bypass these without external current limiting and scope verification.
- ADC-derived thresholds near `0x0A1A`, `0x0A1E`, `0x0E54`, and the calibration block `0x0D50..0x0D7B` appear to determine output permissives, current/fault limits, and regulation behavior.
- State `4` deliberately disables interrupts and returns to the bootloader.  Treat it as a controlled reboot/update path, not just an error state.

## Rebuild notes

The provided C is designed to be readable and editable.  To produce a binary that can replace the stock image, add a linker script and vector/startup stubs that preserve either the low-image or high-image memory layout.  For experimental behavior changes, start by instrumenting the existing binary behavior: PWM period/duty, ADC trigger timing, I2C/UART frames, and fault-trip transitions.


## Rev.06 vs Rev.10 comparison note

This Rev.06 package was compared against the previously supplied stock Rev.10 secondary firmware.  No program-memory, decoded-HEX, or configuration-bit differences were found.  Snapshot differences in PCL/SR are debug/runtime state only and are recorded in the comparison CSV files.
