# Reverse-engineering notes

## Reset and startup

- `0x0000`: `GOTO 0x0200`
- `0x0200`: stack initialization (`W15=0x0C78`, `SPLIM=0x0FDC`), C data init, then `CALL 0x1AB8`.

## Main loop high-level pseudocode

```c
init_clock();
init_gpio();
init_timer1();
init_uart();
control_reset();
init_adc();
init_comparator1();
wait_ticks(2); ADCON.ADON = 1;
wait_ticks(2); PTCON.PTEN = 1;
wait_ticks(1);
for (;;) {
    while (!(G_COMM_FLAGS & COMM_TX_OK)) {
        send_status_frame();
        wait_ticks(2);
    }
    control_update();
    line_target_update();
    if ((G_STATUS_FLAGS & STATUS_CHANGED) && (G_COMM_FLAGS & COMM_TX_OK))
        send_status_frame();
    update_sine_envelope();
    state_machine();
    housekeeping();
    math_housekeeping();
}
```

## Safety-critical observations

- The trap handlers at `0x23B6..0x23CE` clear `PTCON` high-byte bit 7 before looping, so fault traps intentionally stop PWM.
- `CMPDAC1` is normally set to `0x03FF`; some active-control branches set it to `0x0294`.
- PWM duty is ultimately written through `PDC1`; `TRIG1` is set to half of `PDC1`, synchronizing ADC trigger timing to duty.
- The UART command handler writes into a contiguous 16-bit RAM window beginning at `0x0BC8`; this makes host-side modification of run/fault/setpoint variables possible.
