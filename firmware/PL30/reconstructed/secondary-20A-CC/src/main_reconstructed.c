/*
 * PL30 secondary-side PSU firmware reconstruction, 20A CC Rev.06 patch pass.
 * Target: Microchip dsPIC33FJ64GS606, MPLAB X / XC16.
 *
 * This is not Microchip or OEM source.  It is an editable reconstruction from
 * the 20A CC modified PL30 Rev.06 secondary-side program-memory, SFR, config-bit, and RAM
 * dumps.  Addresses from the disassembly are retained in function names and
 * comments so each block can be correlated back to the stock image.
 *
 * The firmware contains a boot/update selector at 0x5000 and two application
 * images: a low image starting at 0x0400 and a high duplicate starting at 0x6000.
 * The C below models the application control behavior, not a bit-identical
 * binary.  Validate on an isolated/current-limited bench setup before connecting
 * to the real power stage.
 */
#include <xc.h>
#include <stdint.h>
#include <stdbool.h>

#include "pl30sec_symbols.h"
#include "app_initial_data.h"

#ifndef PL30SEC_RECON_HAS_MAIN
#define PL30SEC_RECON_HAS_MAIN 1
#endif


/* --- 20A CC Rev.06 patch knobs -------------------------------------------------
 * These constants correspond directly to the six changed program words found when
 * comparing this image against stock Rev.06/Rev.10.  The high application image
 * carries the same changes at +0x5C00.
 */
#define CC20_STOCK_VOUT_FAULT_THRESHOLD_COUNTS 0x0370u
#define CC20_VOUT_FAULT_THRESHOLD_COUNTS       0x03B2u
#define CC20_STOCK_MODE1_E50_LIMIT_WORD        0x5511u
#define CC20_MODE1_E50_LIMIT_WORD              0x1000u
#define CC20_STOCK_OUTPUT_SETPOINT_FLOAT_BITS  0x46C3EF9Eul
#define CC20_OUTPUT_SETPOINT_FLOAT_BITS        0x46E8EF9Eul
#define CC20_STOCK_OUTPUT_SETPOINT_FLOAT       25079.80859375f
#define CC20_OUTPUT_SETPOINT_FLOAT             29815.80859375f
#define CC20_OUTPUT_MIN_FLOAT                  24268.80078125f   /* 0x46BD999A */
#define CC20_OUTPUT_ALT_FLOAT                  24678.400390625f  /* 0x46C0CCCD, unchanged */

static void init_clock_0x43C4(void);
static void init_gpio_0x4404(void);
static void init_timers_0x4442(void);
static void init_uart1_0x2328(void);
static void init_pwm3_0x3FBA(void);
static void start_pwm3_gate_0x3FDE(void);
static void init_pwm4_pwm6_0x3C16(void);
static void init_pwm4_softstart_0x30EC(void);
static void init_adc_0x4452(void);
static void load_calibration_0x14EC(void);
static void refresh_calibration_0x167A(void);
static void clear_i2c2_flags_0x1964(void);
static void init_i2c2_0x196A(void);
static void select_i2c1_address_0x07D8(void);
static void uart1_housekeeping_0x2350(void);
static void comm_fault_latch_0x3DAA(void);
static void evaluate_output_faults_0x4124(void);
static void pwm6_control_update_0x34F4(void);
static void fault_counter_a_service_0x3DD2(void);
static void fault_counter_b_service_0x3E10(void);
static void pwm_ramp_update_0x3E48(void);
static void state_gate_update_0x3E64(void);
static void status_latch_update_0x3EB4(void);
static void state_machine_0x28A4(void);
static void cc20_voltage_threshold_service_0x3340(void);
static void cc20_mode_limit_table_0x3422(void);
static void cc20_output_setpoint_service_0x393A(void);
static void i2c2_transaction_service_0x3F46(void);
static void wait_ticks_0x3D82(uint16_t ticks);
static void clear_runtime_shutdown_state_0x3D8E(void);
static void shutdown_and_jump_bootloader_0x3F1E(void) __attribute__((noreturn));

static inline uint16_t lowpass_3_4(uint16_t old_value, uint16_t sample)
{
    return (uint16_t)((((uint32_t)old_value * 3u) + (uint32_t)sample) >> 2);
}

static inline void uart_wait_tx_ready(void)
{
    while ((SEC_U1STA & 0x0001u) == 0u) {
        ;
    }
}

static inline void uart_send_byte(uint8_t b)
{
    uart_wait_tx_ready();
    SEC_U1TXREG = b;
}

#if PL30SEC_RECON_HAS_MAIN
int main(void)
{
    /* Low image 0x3C44 sets the global interrupt-control flag first.  The high
     * duplicate at 0x9844 clears it before using the same initialization body.
     * The hardware IVT in the stock image normally points at the high copy.
     */
    SEC_GLOBAL_INT_FLAGS |= 0x80u;

    init_clock_0x43C4();
    /* 0x3EE4 reads/refreshes boot-marker/config flash shadow at 0x5C00. */
    clear_runtime_shutdown_state_0x3D8E();
    init_gpio_0x4404();
    init_timers_0x4442();
    init_uart1_0x2328();

    wait_ticks_0x3D82(1u);
    start_pwm3_gate_0x3FDE();
    init_pwm4_pwm6_0x3C16();
    init_pwm4_softstart_0x30EC();
    init_adc_0x4452();

    SEC_POWER_FLAGS &= (uint16_t)~(SEC_POWER_FLAG_OUTPUT_OK | SEC_POWER_FLAG_FAULT3 | SEC_POWER_FLAG_CHANGED);
    SEC_POWER_FLAGS_H &= (uint8_t)~0x80u;

    load_calibration_0x14EC();

    SEC_ADCON |= 0x8000u;      /* ADCON high-byte bit7 observed after calibration load */
    wait_ticks_0x3D82(1u);
    SEC_PTCON |= 0x8000u;      /* PTCON high-byte bit7 enables PWM time base */
    wait_ticks_0x3D82(1u);

    clear_i2c2_flags_0x1964();
    init_i2c2_0x196A();

    for (;;) {
        select_i2c1_address_0x07D8();
        uart1_housekeeping_0x2350();
        comm_fault_latch_0x3DAA();
        evaluate_output_faults_0x4124();
        pwm6_control_update_0x34F4();
        refresh_calibration_0x167A();
        fault_counter_a_service_0x3DD2();
        fault_counter_b_service_0x3E10();
        pwm_ramp_update_0x3E48();
        state_gate_update_0x3E64();
        status_latch_update_0x3EB4();
        state_machine_0x28A4();
        i2c2_transaction_service_0x3F46();

        switch ((sec_app_state_t)SEC_APP_STATE) {
        case SEC_APP_STATE_IDLE:
            /* Normal low-power/await-command state.  Output remains inhibited
             * until UART/I2C command windows and ADC permissives request run.
             */
            break;

        case SEC_APP_STATE_PENDING_RUN:
            SEC_STATUS_LATCH_88A |= PL30SEC_BIT(0);
            break;

        case SEC_APP_STATE_RAMP_OR_RUN:
            /* PWM6/PWM4 regulation and ADC protection happen in the service
             * calls above; state 3 mostly controls gating/latching behavior.
             */
            break;

        case SEC_APP_STATE_BOOT_SHUTDOWN:
            shutdown_and_jump_bootloader_0x3F1E();
            break;

        default:
            SEC_APP_STATE = SEC_APP_STATE_IDLE;
            break;
        }
    }
}
#endif

static void wait_ticks_0x3D82(uint16_t ticks)
{
    SEC_TICK_COUNTER = 0u;
    while (SEC_TICK_COUNTER < ticks) {
        ;
    }
}

static void init_clock_0x43C4(void)
{
    SEC_PLLFBD = 0x0029u;
    SEC_CLKDIV &= 0xE03Fu;

    /* Stock uses the OSCCON unlock sequence at 0x742/0x743 and waits for COSC
     * and LOCK.  Use XC16 builtins where available for source-level rebuilds.
     */
#if defined(__XC16__)
    __builtin_write_OSCCONH(0x01u);
    __builtin_write_OSCCONL((uint8_t)(SEC_OSCCON | 0x01u));
#endif
    while ((SEC_OSCCON & 0x0070u) != 0x0010u) { ; }
    while ((SEC_OSCCON & 0x0020u) == 0u) { ; }

    SEC_ACLKCON = 0xA740u;
    SEC_PTCON2 = 0x0001u;
}

static void init_gpio_0x4404(void)
{
    /* Direct LAT/TRIS names are used intentionally: they are clearer in MPLAB
     * than the byte aliases in the raw disassembly.
     */
#if defined(LATB)
    LATB = 0x0000u;  TRISB = 0x3FDFu;
    LATC = 0x0000u;  TRISC = ((SEC_BOOT_OR_BOARD_FLAGS & PL30SEC_BIT(1)) != 0u) ? 0x6FFFu : 0xEFFFu;
    LATD = 0x0030u;  TRISD = 0xFF01u;
    LATE = 0x0000u;  TRISE = 0xFFF0u;
    LATF = 0x0000u;  TRISF = 0xFFBFu;
    LATG = 0x0040u;  TRISG = 0xFC3Fu;
#else
    SEC_LATC_HIGH |= 0x10u;
    SEC_LATD_LOW  |= 0x20u;
    SEC_LATD_LOW  |= 0x10u;
    SEC_LATG_LOW  |= 0x40u;
#endif
}

static void init_timers_0x4442(void)
{
#if defined(PR1)
    PR1 = 0x9C40u;
    T1CON |= 0x8000u;
    PR2 = 0x1F40u;
    T2CON |= 0x8000u;
#else
    PL30SEC_REG16(0x0102u) = 0x9C40u;
    PL30SEC_SET_BIT8(0x0105u, 7);
    PL30SEC_REG16(0x010Cu) = 0x1F40u;
    PL30SEC_SET_BIT8(0x0111u, 7);
#endif
    SEC_IEC0 |= PL30SEC_BIT(3);        /* Timer1 interrupt enable */
}

static void init_uart1_0x2328(void)
{
    SEC_U1BRG = 0x001Au;
    SEC_U1MODE = 0x0000u;
    SEC_U1STA = 0x0000u;
    SEC_UART_TIMEOUT_TICK = 0u;
    SEC_UART_STATUS_WORD |= 0x0080u;
    SEC_UART_RETRY_STATE = 5u;
    PL30SEC_REG16(0x09CEu) = 5u;
    PL30SEC_REG16(0x09D0u) = 5u;

    SEC_U1MODE |= 0x8000u;
    SEC_U1STA  |= 0x0400u;
    SEC_IEC0   |= PL30SEC_BIT(11);     /* U1RX, inferred from vector table */
}

static void init_pwm3_0x3FBA(void)
{
    if ((SEC_BOOT_OR_BOARD_FLAGS & PL30SEC_BIT(1)) != 0u) {
        PL30SEC_SET_BIT8(0x0463u, 7);  /* high byte of PWMCON3/IOCON3 path seen in disasm */
    }
    SEC_PWMCON3 = 0x0000u;
    SEC_FCLCON3 = 0x0000u;
    SEC_PHASE3 = 0x3FFFu;
    SEC_PDC3 = 0x0000u;
}

static void start_pwm3_gate_0x3FDE(void)
{
    SEC_LATC_HIGH |= 0x80u;
    init_pwm3_0x3FBA();
    SEC_OUTPUT_HOLD_FLAGS &= (uint16_t)~SEC_OUTPUT_HOLD_1;
    SEC_PWM_ENABLE_FLAGS |= SEC_PWM_ENABLE_0;
    SEC_OUTPUT_HOLD_FLAGS &= (uint16_t)~SEC_OUTPUT_HOLD_2;
}

static void init_pwm4_pwm6_0x3C16(void)
{
    PL30SEC_SET_BIT8(0x0483u, 7);
    SEC_PHASE4 = 0x8214u;
    SEC_PDC4 = 0x0000u;

    SEC_PWMCON6 = 0x0000u;
    SEC_PHASE6 = 0x3FFFu;
    SEC_PDC6 = 0x445Cu;
    SEC_PWM_ENABLE_FLAGS = 0x000Fu;
    PL30SEC_REG16(0x0EB4u) = 0u;
}

static void init_pwm4_softstart_0x30EC(void)
{
    PL30SEC_SET_BIT8(0x0483u, 6);
    SEC_SPHASE4 = 0x4A38u;
    SEC_SDC4 = 0x0514u;
    PL30SEC_REG16(0x0828u) = 0u;
    PL30SEC_REG16(0x082Au) = 0u;
    PL30SEC_REG16(0x0838u) = 0u;
}

static void init_adc_0x4452(void)
{
    SEC_ADCON = 0x0000u;
    SEC_ADPCFG = 0xF000u;
    SEC_ADSTAT = 0x0000u;

    /* Exact ADCPC values should be verified against the disassembly before a
     * rebuild; these placeholders keep channel-pair/PWM trigger setup visible.
     */
    SEC_ADCPC0 = 0x0000u;
    SEC_ADCPC1 = 0x0000u;
    SEC_ADCPC2 = 0x0000u;
    SEC_ADCPC3 = 0x0000u;
    SEC_IEC7 |= (PL30SEC_BIT(1) | PL30SEC_BIT(2) | PL30SEC_BIT(3));
}

static void load_calibration_0x14EC(void)
{
    /* The stock routine reads external configuration/calibration through the
     * I2C2 transaction engine into 0x0D50..0x0D7B.  This source keeps the data
     * flow explicit.  Replace the conservative defaults after bench-verifying
     * your EEPROM/register map.
     */
    if (SEC_CAL_VALID_DIRTY == 0u) {
        SEC_CAL_0D50 = SEC_CAL_0D50;
    }
}

static void refresh_calibration_0x167A(void)
{
    if (SEC_CAL_VALID_DIRTY != 0u) {
        load_calibration_0x14EC();
        SEC_CAL_VALID_DIRTY = 0u;
    }
}

static void clear_i2c2_flags_0x1964(void)
{
    SEC_I2C2_FLAGS &= (uint16_t)~PL30SEC_BIT(0);
    SEC_I2C2_SUBSTATE = 0u;
}

static void init_i2c2_0x196A(void)
{
    if ((SEC_I2C2_FLAGS & PL30SEC_BIT(0)) != 0u) {
        return;
    }
    SEC_I2C2CON = 0x8200u;
    SEC_I2C2BRG = 0x018Bu;
    SEC_IFS3 &= (uint16_t)~PL30SEC_BIT(2);
    SEC_IEC3 |= PL30SEC_BIT(2);
    SEC_I2C2_FLAGS |= PL30SEC_BIT(0);
}

static void select_i2c1_address_0x07D8(void)
{
    if ((SEC_ADC_AUX_VALID_MODE == 1u) && (SEC_I2C1_ADDRESS_DELAY > 0x01F3u)) {
        uint16_t addr = SEC_I2C1_BOARD_ID & 0x007Fu;
#if defined(PORTD)
        addr ^= (PORTD & 0x0007u);
#endif
        SEC_I2C1ADD = addr;
        SEC_I2C1CON |= 0x8000u;
    }
}

static void uart_send_query_frame_0x1F30(uint8_t command)
{
    uart_send_byte(0x05u);
    uart_send_byte(command);
    SEC_UART_FRAME_STATE = 1u;
    SEC_UART_FLAGS |= SEC_UART_FLAG_BUSY;
    SEC_UART_FLAGS &= (uint16_t)~SEC_UART_FLAG_VALUE;
    SEC_UART_LAST_COMMAND = command;
}

static void uart_send_param_frame_0x1F54(uint8_t command, uint8_t value)
{
    uart_send_byte(0x50u);
    uart_send_byte(command);
    uart_send_byte(value);
    SEC_UART_FRAME_STATE = 1u;
    SEC_UART_FLAGS |= (SEC_UART_FLAG_BUSY | SEC_UART_FLAG_VALUE);
    SEC_UART_LAST_COMMAND = command;
    SEC_UART_LAST_VALUE = value;
}

static void uart1_housekeeping_0x2350(void)
{
    if (SEC_UART_TIMEOUT_TICK > 0x0012u) {
        SEC_UART_FLAGS &= (uint16_t)~SEC_UART_FLAG_TIMEOUT;
        SEC_UART_TIMEOUT_TICK = 0u;
    }

    if ((SEC_UART_RETRY_STATE & PL30SEC_BIT(2)) != 0u) {
        SEC_UART_RETRY_STATE &= (uint16_t)~PL30SEC_BIT(2);
        SEC_UART_FLAGS |= SEC_UART_FLAG_EVENT5;
        uart_send_query_frame_0x1F30((uint8_t)(SEC_UART_LAST_COMMAND & 0xFFu));
    }
}

static void comm_fault_latch_0x3DAA(void)
{
    if (((SEC_STATUS_LATCH_88A & PL30SEC_BIT(4)) != 0u) &&
        ((SEC_UART_STATUS_WORD & PL30SEC_BIT(1)) != 0u)) {
        SEC_POWER_FLAGS |= SEC_POWER_FLAG_CHANGED;
    }
}

static void evaluate_output_faults_0x4124(void)
{
    if ((SEC_POWER_FLAGS & SEC_POWER_FLAG_FAULT3) != 0u) {
        SEC_PWM_ENABLE_FLAGS &= (uint16_t)~SEC_PWM_ENABLE_0;
        SEC_PDC3 = 0u;
        return;
    }

    if (SEC_ADC_AUX9_FILTERED > 0x033Eu) {
        SEC_POWER_FLAGS |= SEC_POWER_FLAG_OUTPUT_OK;
        SEC_PWM_ENABLE_FLAGS |= SEC_PWM_ENABLE_0;
    } else {
        SEC_POWER_FLAGS &= (uint16_t)~SEC_POWER_FLAG_OUTPUT_OK;
        SEC_PWM_ENABLE_FLAGS &= (uint16_t)~SEC_PWM_ENABLE_0;
    }
}

static void pwm6_control_update_0x34F4(void)
{
    cc20_voltage_threshold_service_0x3340();
    cc20_mode_limit_table_0x3422();

    uint16_t feedback = SEC_ADC_VBUS_FILTERED;
    uint16_t target = PL30SEC_REG16(0x0D4Au);

    if ((SEC_PWM6_LOOP_FLAGS & PL30SEC_BIT(5)) != 0u) {
        if (feedback < target && SEC_PDC6 < 0x3FFEu) {
            SEC_PDC6++;
        } else if (feedback > target && SEC_PDC6 > 0u) {
            SEC_PDC6--;
        }
    }
}


static void cc20_voltage_threshold_service_0x3340(void)
{
    /* Low address 0x3348 / high mirror 0x8F48:
     *     stock: MOV #0x0370, W0
     *     CC20:  MOV #0x03B2, W0
     * The surrounding code compares filtered output-voltage feedback at 0x0A02
     * and PHASE1 against this threshold before setting shutdown/status latches.
     */
    if (SEC_APP_STATE != SEC_APP_STATE_RAMP_OR_RUN) {
        return;
    }

    if (SEC_ADC_VBUS_FILTERED > CC20_VOUT_FAULT_THRESHOLD_COUNTS) {
        const uint16_t phase_threshold =
            (uint16_t)(CC20_VOUT_FAULT_THRESHOLD_COUNTS + 0x03E3u);
        if (SEC_PHASE1 > phase_threshold) {
            SEC_PWM_ENABLE_FLAGS |= PL30SEC_BIT(5);
            SEC_STATUS_LATCH_88A |= PL30SEC_BIT(1);
            SEC_STATUS_LATCH_88C |= PL30SEC_BIT(2);
            SEC_CC_VOUT_PREDICT_894 = (uint16_t)((SEC_ADC_VBUS_FAST << 2) |
                                                 (SEC_ADC_VBUS_FAST >> 4));
        }
    }

    /* The nearby hysteresis flag at 0x336A/0x8F6A is unchanged. */
    if (SEC_ADC_VBUS_FILTERED > 0x0348u) {
        PL30SEC_REG16(0x09A6u) |= PL30SEC_BIT(5);
    } else if (SEC_ADC_VBUS_FILTERED <= 0x0329u) {
        PL30SEC_REG16(0x09A6u) &= (uint16_t)~PL30SEC_BIT(5);
    }
}

static void cc20_mode_limit_table_0x3422(void)
{
    /* Low address 0x3446 / high mirror 0x9046:
     * The mode selector is ((SEC_POWER_FLAGS >> 13) & 3).  Only mode 1 changed;
     * SEC_CC_MODE_E50_LIMIT is likely a current-loop/current-limit reference.
     */
    const uint16_t mode = (uint16_t)((SEC_POWER_FLAGS >> 13) & 0x0003u);

    switch (mode) {
    case 0u:
        SEC_CC_MODE_A_CCE     = 0x1573u;
        SEC_CC_MODE_E50_LIMIT = 0x4B9Eu;
        SEC_CC_MODE_EA6_LIMIT = 0x1080u;
        break;
    case 1u:
        SEC_CC_MODE_A_CCE     = 0x1860u;
        SEC_CC_MODE_E50_LIMIT = CC20_MODE1_E50_LIMIT_WORD; /* stock: 0x5511 */
        SEC_CC_MODE_EA6_LIMIT = 0x12C0u;
        break;
    case 2u:
        SEC_CC_MODE_A_CCE     = 0x2080u;
        SEC_CC_MODE_E50_LIMIT = 0x716Eu;
        SEC_CC_MODE_EA6_LIMIT = 0x1900u;
        break;
    default:
        SEC_POWER_FLAGS_H &= (uint8_t)~(PL30SEC_BIT(5) | PL30SEC_BIT(6));
        break;
    }

    SEC_CC_MODE_APPLIED_CCC = SEC_CC_MODE_A_CCE;
    if ((SEC_STATUS_LATCH_88A & PL30SEC_BIT(0)) != 0u && SEC_RAMP_COUNTER < SEC_CC_MODE_APPLIED_CCC) {
        SEC_CC_MODE_APPLIED_CCC = SEC_RAMP_COUNTER;
    }
}

static void cc20_output_setpoint_service_0x393A(void)
{
    /* Low address 0x39B4 / high mirror 0x95B4:
     *     stock: 0x46C3EF9E = 25079.80859375f
     *     CC20:  0x46E8EF9E = 29815.80859375f
     * The surrounding code uses XC16 float helper calls before writing 0x09EC.
     */
    const bool alternate_range = ((SEC_POWER_FLAGS & PL30SEC_BIT(1)) != 0u);
    const float setpoint_scale = alternate_range ? CC20_OUTPUT_ALT_FLOAT : CC20_OUTPUT_SETPOINT_FLOAT;
    const float command = (float)(int16_t)SEC_VOUT_COMMAND_WORD;
    float target = command * (setpoint_scale / 32768.0f);

    SEC_OUTPUT_FREQ_OR_TIMER = alternate_range ? 0x0898u : 0x0FA0u;

    if (target < CC20_OUTPUT_MIN_FLOAT) {
        target = CC20_OUTPUT_MIN_FLOAT;
    }
    if (target > 65535.0f) {
        target = 65535.0f;
    }

    SEC_VOUT_TARGET_COUNTS = (uint16_t)target;
}

static void fault_counter_a_service_0x3DD2(void)
{
    if (SEC_FAULT_COUNTER_A > 0x000Au) {
        SEC_PWM_ENABLE_FLAGS |= SEC_PWM_ENABLE_1;
    }
}

static void fault_counter_b_service_0x3E10(void)
{
    if (SEC_FAULT_COUNTER_B > 0x0257u) {
        SEC_POWER_FLAGS |= SEC_POWER_FLAG_CHANGED;
    }
}

static void pwm_ramp_update_0x3E48(void)
{
    cc20_output_setpoint_service_0x393A();

    if (SEC_APP_STATE == SEC_APP_STATE_RAMP_OR_RUN && SEC_RAMP_COUNTER != 0u) {
        SEC_RAMP_COUNTER--;
    }
}

static void state_gate_update_0x3E64(void)
{
    if (SEC_APP_STATE == SEC_APP_STATE_IDLE) {
        SEC_PDC6 = 0x445Cu;
    }
}

static void status_latch_update_0x3EB4(void)
{
    if ((SEC_STATUS_LATCH_88C & PL30SEC_BIT(5)) != 0u) {
        SEC_POWER_FLAGS |= SEC_POWER_FLAG_CHANGED;
    }
}

static void state_machine_0x28A4(void)
{
    switch ((sec_app_state_t)SEC_APP_STATE) {
    case SEC_APP_STATE_IDLE:
        if ((SEC_STATUS_LATCH_88A & PL30SEC_BIT(0)) != 0u) {
            SEC_APP_STATE = SEC_APP_STATE_PENDING_RUN;
        }
        break;
    case SEC_APP_STATE_PENDING_RUN:
        SEC_APP_STATE = SEC_APP_STATE_RAMP_OR_RUN;
        break;
    case SEC_APP_STATE_RAMP_OR_RUN:
        if ((SEC_POWER_FLAGS & SEC_POWER_FLAG_FAULT3) != 0u) {
            SEC_APP_STATE = SEC_APP_STATE_IDLE;
        }
        break;
    case SEC_APP_STATE_BOOT_SHUTDOWN:
    default:
        break;
    }
}

static void i2c2_transaction_service_0x3F46(void)
{
    if ((SEC_I2C2_FLAGS & PL30SEC_BIT(3)) == 0u) {
        return;
    }
    if (SEC_I2C2_TIMEOUT > 5u && (SEC_I2C2STAT & 0x0002u) == 0u) {
        SEC_I2C2_SUBSTATE = 0u;
        SEC_I2C2_FLAGS &= (uint16_t)~PL30SEC_BIT(3);
    }
}

static void clear_runtime_shutdown_state_0x3D8E(void)
{
    SEC_PWM_ENABLE_FLAGS &= 0x000Fu;
    SEC_RAMP_COUNTER = 0u;
    SEC_STATUS_LATCH_88A &= 0x000Fu;
    SEC_STATUS_LATCH_88C = 0u;
    PL30SEC_REG16(0x0E1Au) = 0u;
    PL30SEC_CLR_BIT16(0x08C0u, 5);
    PL30SEC_CLR_BIT8(0x02E0u, 1);
    PL30SEC_REG16(0x0E14u) = 0u;
    PL30SEC_REG16(0x0E16u) = 0u;
    PL30SEC_REG16(0x0E18u) = 0u;
}

static void shutdown_and_jump_bootloader_0x3F1E(void)
{
    SEC_IEC0 &= (uint16_t)~PL30SEC_BIT(3);
    SEC_IEC1 &= (uint16_t)~PL30SEC_BIT(0);
    SEC_IEC3 &= (uint16_t)~PL30SEC_BIT(2);
    SEC_PTCON &= (uint16_t)~0x8000u;
    SEC_PDC3 = 0u;
    SEC_PDC4 = 0u;
    SEC_PDC6 = 0u;
    clear_runtime_shutdown_state_0x3D8E();

    ((void (*)(void))0x5000u)();
    for (;;) { ; }
}

/* Interrupt bodies reconstructed at behavior level.  For a rebuild, connect
 * them to XC16 interrupt attributes or to an assembly vector table matching the
 * selected low/high image.
 */
void sec_timer1_housekeeping_isr_0x28AA(void)
{
    SEC_TICK_COUNTER++;
    SEC_UART_TIMEOUT_TICK++;
    SEC_I2C1_ADDRESS_DELAY++;
}

void sec_uart1_rx_isr_0x20D4(void)
{
    uint8_t rx = (uint8_t)SEC_U1RXREG;
    static uint8_t header;
    static uint8_t cmd;

    if (rx == 0x05u || rx == 0x50u) {
        header = rx;
        SEC_UART_FRAME_STATE = 1u;
        return;
    }

    if (SEC_UART_FRAME_STATE == 1u) {
        cmd = rx;
        SEC_UART_LAST_COMMAND = cmd;
        SEC_UART_FRAME_STATE = (header == 0x50u) ? 2u : 0u;
        if (header == 0x05u) {
            SEC_UART_FLAGS &= (uint16_t)~SEC_UART_FLAG_BUSY;
        }
        return;
    }

    if (SEC_UART_FRAME_STATE == 2u) {
        SEC_UART_LAST_VALUE = rx;
        PL30SEC_REG16((uint16_t)(SEC_UART_STATUS_BASE + ((uint16_t)cmd * 2u))) = rx;
        SEC_UART_FRAME_STATE = 0u;
        SEC_UART_FLAGS &= (uint16_t)~SEC_UART_FLAG_BUSY;
    }
}

void sec_adc_isr_current_pair_0x2BD0(void)
{
    SEC_ADC_CURRENT_RAW_A = SEC_ADCBUF6;
    SEC_ADC_CURRENT_RAW_B = SEC_ADCBUF7;
    SEC_ADC_CURRENT_FILTER_A = lowpass_3_4(SEC_ADC_CURRENT_FILTER_A, SEC_ADC_CURRENT_RAW_A);
    SEC_ADC_CURRENT_FILTER_B = lowpass_3_4(SEC_ADC_CURRENT_FILTER_B, SEC_ADC_CURRENT_RAW_B);
    SEC_ADSTAT &= (uint16_t)~PL30SEC_BIT(3);
    SEC_IFS7 &= (uint16_t)~PL30SEC_BIT(3);
}

void sec_adc_isr_pwm6_pair_0x34FE(void)
{
    uint16_t sample = SEC_ADCBUF1;
    if (sample > PL30SEC_REG16(0x0DDCu)) {
        SEC_POWER_FLAGS |= SEC_POWER_FLAG_FAULT3;
    }
    pwm6_control_update_0x34F4();
    SEC_IFS7 &= (uint16_t)~PL30SEC_BIT(1);
}

void sec_adc_isr_vbus_pair_0x3858(void)
{
    SEC_ADC_VBUS_FAST = SEC_ADCBUF2;
    SEC_ADC_VBUS_FILTERED = lowpass_3_4(SEC_ADC_VBUS_FILTERED, SEC_ADC_VBUS_FAST);
    PL30SEC_REG16(0x0A4Au) = SEC_ADCBUF3;
    SEC_POWER_FLAGS_H |= 0x80u;
    SEC_ADSTAT &= (uint16_t)~PL30SEC_BIT(1);
    SEC_IFS7 &= (uint16_t)~PL30SEC_BIT(7);
}

void sec_adc_isr_aux_pair_0x4162(void)
{
    SEC_ADC_AUX8_RAW = SEC_ADCBUF8;
    SEC_ADC_AUX9_RAW = SEC_ADCBUF9;
    SEC_ADC_AUX4_RAW = SEC_ADCBUF4;
    SEC_ADC_AUX_VALID_MODE = (SEC_ADC_AUX8_RAW > 0x02C0u && SEC_ADC_AUX8_RAW < 0x02D0u) ? 1u : 0u;
    SEC_ADC_AUX8_FILTERED = lowpass_3_4(SEC_ADC_AUX8_FILTERED, SEC_ADC_AUX8_RAW);
    SEC_ADC_AUX9_FILTERED = lowpass_3_4(SEC_ADC_AUX9_FILTERED, SEC_ADC_AUX9_RAW);
    SEC_ADSTAT &= (uint16_t)~PL30SEC_BIT(4);
    SEC_IFS7 &= (uint16_t)~PL30SEC_BIT(2);
}
