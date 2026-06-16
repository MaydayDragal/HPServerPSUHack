/*
 * PICO/Raspberry-Pi external-control hooks reconstructed from the modified
 * PL30 Rev.10 secondary dsPIC33FJ64GS606 image.
 *
 * These functions model the inserted helper blocks at 0x5B00..0x5B76 and the
 * mirrored high-image blocks at 0xA220..0xA2C6.  They are deliberately small and
 * address-oriented so future edits can be translated back into patch points.
 */
#include <xc.h>
#include <stdint.h>
#include <stdbool.h>

#include "pl30sec_symbols.h"
#include "pico_control_patch_points.h"

static inline uint16_t clamp_u16(uint16_t value, uint16_t lo, uint16_t hi)
{
    if (value < lo) {
        return lo;
    }
    if (value > hi) {
        return hi;
    }
    return value;
}

/* 0x5B00 low image / 0xA220 high mirror.
 * Replaces a raw MOV ADCBUF2,W1 in the output-voltage/OVP sense path.
 */
uint16_t pico_scaled_vout_adc_0x5B00_0xA220(void)
{
    return (uint16_t)(((uint32_t)SEC_ADCBUF2 * (uint32_t)PICO_CONTROL_OVP_SCALE_Q15) >> 15);
}

/* 0x5B26 low image / 0xA260 high mirror.
 * I2C logical register 0x1F, wire register 0x3E.  zpsu writes amps * 265.0.
 */
void pico_i2c_write_constant_current_0x5B26_0xA260(void)
{
    SEC_PICO_CC_COMMAND_COUNTS = SEC_I2C1_RX_WORD_OR_PAYLOAD;
}

/* 0x5B20 low image / 0xA250 high mirror.
 * I2C logical register 0x21, wire register 0x42.
 * bit15 = output enable request, bit0 = constant-current mode request.
 */
void pico_i2c_write_status_control_0x5B20_0xA250(void)
{
    SEC_PICO_STATUS_COMMAND = SEC_I2C1_RX_WORD_OR_PAYLOAD;
}

/* 0x5B30 low image / 0xA270 high mirror.
 * Original stock code copied ADCBUF6/ADCBUF7 to RAM 0x0EAA/0x0EAC.  The PICO
 * patch substitutes fixed words depending on the externally written output-enable
 * bit.  This appears to bias/condition the existing current-monitor path for the
 * externally controlled CC mode.
 */
void pico_current_adc_substitute_0x5B30_0xA270(void)
{
    if ((SEC_PICO_STATUS_COMMAND & PICO_CONTROL_STATUS_OUTPUT_ENABLE) != 0u) {
        SEC_ADC_CURRENT_RAW_A_OR_SYNTH = 0x00DEu;
        SEC_ADC_CURRENT_RAW_B_OR_SYNTH = 0x00CCu;
    } else {
        SEC_ADC_CURRENT_RAW_A_OR_SYNTH = 0x03A3u;
        SEC_ADC_CURRENT_RAW_B_OR_SYNTH = 0x0000u;
    }
}

/* 0x5B50 low image / 0xA2A0 high mirror.
 * Applies an externally commanded current limit only while CC mode is requested.
 * The requested value is bounded to a minimum 0x0500 and the stock mode-table
 * ceiling at RAM 0x0E50, then written back into 0x0E50 so the original current
 * control/protection code consumes it without a larger rewrite.
 */
void pico_apply_external_cc_limit_0x5B50_0xA2A0(void)
{
    if ((SEC_PICO_STATUS_COMMAND & PICO_CONTROL_STATUS_CC_ENABLE) != 0u) {
        uint16_t requested = SEC_PICO_CC_COMMAND_COUNTS;
        uint16_t ceiling = SEC_CC_MODE_E50_LIMIT;
        requested = clamp_u16(requested, PICO_CONTROL_CC_MIN_COUNTS, ceiling);
        SEC_PICO_CC_COMMAND_COUNTS = requested;
        SEC_CC_MODE_E50_LIMIT = requested;
    }

    /* Preserve the stock instruction pair that was replaced by RCALL/NOP. */
    SEC_CC_MODE_APPLIED_CCC = SEC_CC_MODE_A_CCE;
}

/* Reconstructed I2C command dispatch patch around the two modified jump-table
 * entries.  The real firmware table indexes the received wire register byte.
 */
void pico_i2c1_command_dispatch_patch(uint8_t wire_register)
{
    switch (wire_register) {
    case PICO_CONTROL_WIRE_REG_CC:
        pico_i2c_write_constant_current_0x5B26_0xA260();
        break;
    case PICO_CONTROL_WIRE_REG_STATUS:
        pico_i2c_write_status_control_0x5B20_0xA250();
        break;
    default:
        /* Stock command table handlers are used for all other registers. */
        break;
    }
}
