#ifndef PICO_CONTROL_PATCH_POINTS_H
#define PICO_CONTROL_PATCH_POINTS_H

#include <stdint.h>

/*
 * Editable constants recovered from the PICO/RPi-control PL30 Rev.10 secondary firmware.
 * The external controller (zpsu/Pico) uses logical registers.  The dsPIC I2C command
 * switch uses the on-wire register byte, which is logical_reg << 1.
 */
#define PICO_CONTROL_LOGICAL_REG_CC        0x1Fu
#define PICO_CONTROL_WIRE_REG_CC           0x3Eu
#define PICO_CONTROL_LOGICAL_REG_STATUS    0x21u
#define PICO_CONTROL_WIRE_REG_STATUS       0x42u

#define PICO_CONTROL_STATUS_CC_ENABLE      0x0001u
#define PICO_CONTROL_STATUS_OUTPUT_ENABLE  0x8000u

#define PICO_CONTROL_CURRENT_SCALE_COUNTS_PER_AMP 265.0f
#define PICO_CONTROL_CC_MIN_COUNTS         0x0500u
#define PICO_CONTROL_CC_MIN_AMPS           ((float)PICO_CONTROL_CC_MIN_COUNTS / PICO_CONTROL_CURRENT_SCALE_COUNTS_PER_AMP)

/* OVP/output-voltage sense scaling: W1 = (ADCBUF2 * 0x6AC2) >> 15 = ADCBUF2 * 0.834045... */
#define PICO_CONTROL_OVP_SCALE_Q15         0x6AC2u
#define PICO_CONTROL_OVP_SCALE_FLOAT       ((float)PICO_CONTROL_OVP_SCALE_Q15 / 32768.0f)

#endif /* PICO_CONTROL_PATCH_POINTS_H */
