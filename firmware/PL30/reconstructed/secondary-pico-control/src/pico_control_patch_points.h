#ifndef PICO_CONTROL_PATCH_POINTS_H
#define PICO_CONTROL_PATCH_POINTS_H

#include <stdint.h>

/*
 * Editable constants recovered from the PICO/RPi-control PL30 Rev.10 secondary firmware.
 * The external controller (zpsu/Pico) uses logical registers.  The dsPIC I2C command
 * switch uses the on-wire register byte, which is logical_reg << 1.
 *
 * I2C write-transaction wire format (5 bytes after the device-address byte):
 *   [WIRE_REG] [PAYLOAD_LO] [PAYLOAD_HI] [CHECKSUM]
 *   CHECKSUM = (0 - WIRE_REG - PAYLOAD_LO - PAYLOAD_HI) & 0xFF
 *
 * Device address: derived at run time from the AT24C01D board-ID EEPROM XOR'd
 * with PORTD[2:0].  Scan the bus to discover it; typical values: 0x40, 0x58.
 */

/* ── I2C logical/wire register numbers ────────────────────────────────── */
#define PICO_CONTROL_LOGICAL_REG_CC        0x1Fu   /* CC setpoint, counts */
#define PICO_CONTROL_WIRE_REG_CC           0x3Eu   /* logical << 1 */
#define PICO_CONTROL_LOGICAL_REG_STATUS    0x21u   /* status/control word */
#define PICO_CONTROL_WIRE_REG_STATUS       0x42u   /* logical << 1 */

/* ── Status/control word (reg 0x21) bit masks ─────────────────────────── */
#define PICO_CONTROL_STATUS_CC_ENABLE      0x0001u /* bit0: apply CC limit */
#define PICO_CONTROL_STATUS_OUTPUT_ENABLE  0x8000u /* bit15: main output on */

/* ── Current scale and range ──────────────────────────────────────────── */
#define PICO_CONTROL_CURRENT_SCALE_COUNTS_PER_AMP 265.0f

/* Minimum CC setpoint — firmware floor enforced by the clamp helper at 0x5B50/0xA2A0. */
#define PICO_CONTROL_CC_MIN_COUNTS         0x0500u   /* 1280 counts */
#define PICO_CONTROL_CC_MIN_AMPS           ((float)PICO_CONTROL_CC_MIN_COUNTS / PICO_CONTROL_CURRENT_SCALE_COUNTS_PER_AMP)  /* ≈ 4.83 A */

/*
 * Maximum CC setpoint — determined at run time by the stock mode-table ceiling
 * at RAM 0x0E50.  There is no hard-coded upper bound in the patch; the existing
 * current-limit machinery sets 0x0E50 each control cycle.  For a 1200 W / 12 V
 * unit the practical ceiling is near 100 A, but may be lower depending on
 * operating state and calibration.
 */

/* Convenience integer version of the scale factor (x10 for integer arithmetic). */
#define PICO_CONTROL_CURRENT_SCALE_X10     2650u

/* Convert between amps and ADC counts at compile time (float). */
#define PICO_AMPS_TO_COUNTS(a)   ((uint16_t)((float)(a) * PICO_CONTROL_CURRENT_SCALE_COUNTS_PER_AMP + 0.5f))
#define PICO_COUNTS_TO_AMPS(c)   ((float)(c) / PICO_CONTROL_CURRENT_SCALE_COUNTS_PER_AMP)

/* ── Synthetic current-ADC shadow values (current-ADC substitute helper) ─ */
/*
 * The patch replaces the raw ADCBUF6/ADCBUF7 copy with fixed bias words to
 * prevent the stock protection path from tripping while the external CC loop
 * controls current.  These are NOT real current readings.
 */
#define PICO_CONTROL_CURRENT_SHADOW_DISABLED_A   0x03A3u  /* 931 counts ≈ 3.5 A — high-idle bias, output off */
#define PICO_CONTROL_CURRENT_SHADOW_DISABLED_B   0x0000u
#define PICO_CONTROL_CURRENT_SHADOW_ENABLED_A    0x00DEu  /* 222 counts ≈ 0.84 A — low-on bias, output on  */
#define PICO_CONTROL_CURRENT_SHADOW_ENABLED_B    0x00CCu  /* 204 counts (secondary sense channel) */

/* ── OVP/output-voltage sense scaling ────────────────────────────────── */
/*
 * Applied at 0x385C/0x945C, replacing a raw MOV ADCBUF2,W1:
 *   effective_adc = (ADCBUF2 * PICO_CONTROL_OVP_SCALE_Q15) >> 15
 * This compensates for the OVP resistor-divider hardware mod (220 Ω → 182 Ω).
 * Without the hardware mod the OVP trips at ~83% of the nominal threshold.
 */
#define PICO_CONTROL_OVP_SCALE_Q15         0x6AC2u         /* Q1.15 ≈ 0.83405 */
#define PICO_CONTROL_OVP_SCALE_FLOAT       ((float)PICO_CONTROL_OVP_SCALE_Q15 / 32768.0f)

#endif /* PICO_CONTROL_PATCH_POINTS_H */
