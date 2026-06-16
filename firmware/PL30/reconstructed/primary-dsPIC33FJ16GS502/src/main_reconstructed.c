/*
 * PL30 primary-side PSU firmware reconstruction, first pass.
 * Target: Microchip dsPIC33FJ16GS502, XC16/MPLAB X.
 *
 * This is not the original source. It is an editable reconstruction from the stock
 * PL30 Rev.10 primary-side program-memory dump. The purpose is to expose the
 * startup sequence, PWM/ADC/comparator setup, state machine, and host protocol so
 * controlled behavior changes can be made and reviewed.
 *
 * For a production binary, validate every safety limit on an isolated bench supply
 * and compare peripheral SFRs, interrupt cadence, and PWM waveforms against the
 * unmodified image before connecting to mains or the real power stage.
 */
#include <xc.h>
#include <stdint.h>
#include <stdbool.h>

#include "pl30_symbols.h"
#include "pl30_sine_table.h"

/* Build option:
 * Define PL30_RECON_HAS_MAIN=1 when importing this as the project entry point.
 * Leave undefined if you want to compile individual functions beside hand-written
 * test harnesses or an assembly vector/startup file.
 */
#ifndef PL30_RECON_HAS_MAIN
#define PL30_RECON_HAS_MAIN 1
#endif

static void init_clock_0x2156(void);
static void init_gpio_0x21E0(void);
static void init_timer1_0x21FE(void);
static void init_uart_0x1454(void);
static void init_filters_and_pwm_defaults_0x05EA(void);
static void init_adc_0x220C(void);
static void init_comparator1_0x21B6(void);
static void wait_ticks_0x1D34(uint16_t ticks);
static void control_update_0x09AA(void);
static void line_target_update_0x0A5E(void);
static void send_status_frame_0x1014(void);
static void update_sine_envelope_from_main_0x1AFE(void);
static void state_machine_from_main_0x1B7A(void);
static void housekeeping_0x0B14(void);
static void math_housekeeping_0x1F5C(void);
static void pwm_control_active_0x0CEE(void);
static void start_pwm_soft_0x0604(void);
static void stop_pwm_soft_0x064A(void);
static void status_side_effects_0x2374(void);
static void host_write_handler_0x1060(void);

static inline void uart_wait_tx_ready(void)
{
    while ((G8(0x0223u) & 0x01u) == 0u) {
        ;
    }
}

static inline void uart_send_byte(uint8_t v)
{
    uart_wait_tx_ready();
    U1TXREG = v;
}

static inline uint16_t wrap_table_index_373(uint16_t v)
{
    while (v > PL30_SINE_TABLE_MAX_INDEX) {
        v = (uint16_t)(v - PL30_SINE_TABLE_LEN);
    }
    return v;
}

static inline uint16_t scaled_sine(uint16_t idx, uint16_t scale)
{
    idx = wrap_table_index_373(idx);
    return (uint16_t)(((uint32_t)PL30_SINE_TABLE[idx] * (uint32_t)scale) >> 7);
}

#if PL30_RECON_HAS_MAIN
int main(void)
{
    init_clock_0x2156();
    init_gpio_0x21E0();
    init_timer1_0x21FE();
    init_uart_0x1454();
    init_filters_and_pwm_defaults_0x05EA();
    init_adc_0x220C();
    init_comparator1_0x21B6();

    wait_ticks_0x1D34(2u);
    ADCON_HIGH |= 0x80u;      /* disasm 0x1ACC: ADCON high byte bit 7 = ADC on */
    wait_ticks_0x1D34(2u);
    PTCON_HIGH |= 0x80u;      /* disasm 0x1AD2: PWM time-base enable */
    wait_ticks_0x1D34(1u);

    while (1) {
        while ((G_COMM_FLAGS & COMM_TX_OK) == 0u) {
            send_status_frame_0x1014();
            wait_ticks_0x1D34(2u);
        }

        control_update_0x09AA();
        line_target_update_0x0A5E();

        if (((G_STATUS_FLAGS & STATUS_CHANGED) != 0u) &&
            ((G_COMM_FLAGS & COMM_TX_OK) != 0u)) {
            send_status_frame_0x1014();
        }

        update_sine_envelope_from_main_0x1AFE();
        state_machine_from_main_0x1B7A();
        housekeeping_0x0B14();
        math_housekeeping_0x1F5C();
    }
}
#endif

static void wait_ticks_0x1D34(uint16_t ticks)
{
    G_DELAY_SUBTICKS = 0u;
    G_DELAY_TICKS = 0u;
    while (G_DELAY_TICKS < ticks) {
        ;
    }
}

static void init_clock_0x2156(void)
{
    /* Original sequence: PLLFBD=0x29, CLKDIV cleanup, OSCCON unlocks, wait for
     * COSC and LOCK, then enable auxiliary clock for high-speed PWM.
     */
    PLLFBD = 0x0029u;
    CLKDIV &= 0xE03Fu;   /* preserve high bits, clear PLLPOST/PLLPRE area as seen */

    __builtin_write_OSCCONH(0x01); /* request FRC+PLL clock selection inferred from OSCCON writes */
    __builtin_write_OSCCONL(OSCCON | 0x01u);
    while ((OSCCON & 0x0070u) != 0x0010u) {
        ;
    }
    while ((OSCCON & 0x0020u) == 0u) {
        ;
    }

    ACLKCON |= 0x0040u;
    G8(0x0751u) |= 0x20u;
    G8(0x0751u) = (uint8_t)((G8(0x0751u) & 0xF8u) | 0x06u);
    do {
        G8(0x0751u) |= 0x80u;
    } while ((G8(0x0751u) & 0x40u) == 0u);
}

static void init_gpio_0x21E0(void)
{
    LATB = 0u;
    G8(0x02CDu) &= (uint8_t)~0x01u; /* LATB bit 8? odd-byte alias used by disassembler */
    G8(0x02CDu) &= (uint8_t)~0x08u;
    LATB &= (uint16_t)~PL30_BIT(5);
    G8(0x02CDu) &= (uint8_t)~0x10u;

    TRISB = 0xA6D3u;
    LATA &= (uint16_t)~PL30_BIT(4);
    TRISA &= (uint16_t)~PL30_BIT(4);
    LATA &= (uint16_t)~PL30_BIT(3);
    TRISA &= (uint16_t)~PL30_BIT(3);
}

static void init_timer1_0x21FE(void)
{
    PR1 = 0x3415u;
    IEC0 |= PL30_BIT(3);
    G8(0x0105u) |= 0x80u;     /* T1CON high byte bit 7, Timer1 ON */
}

static void init_uart_0x1454(void)
{
    /* PPS: U1RX mapped to RP15; U1TX function code 3 mapped through RPOR byte 0x6DD. */
    RPINR18 = (uint16_t)((RPINR18 & 0xFFC0u) | 0x000Fu);
    G8(0x06DDu) = (uint8_t)((G8(0x06DDu) & 0xC0u) | 0x03u);

    U1MODE &= (uint16_t)~PL30_BIT(3);
    U1STA  &= (uint16_t)~PL30_BIT(5);
    U1BRG = 0x001Au;

    G8(0x0085u) &= (uint8_t)~0x08u; /* clear RX flag; exact symbol depends on XC16 header */
    G8(0x00A9u) = (uint8_t)((G8(0x00A9u) & 0x8Fu) | 0x40u);
    G8(0x0095u) |= 0x08u;          /* enable U1RX interrupt */
    G8(0x0221u) |= 0x80u;          /* U1MODE high byte: UART enable */
    G8(0x0223u) |= 0x04u;          /* U1STA high/low alias: transmitter enable */

    G_UART_STATE = 1u;
    G_COMM_FLAGS &= (uint16_t)~COMM_TX_RETRY;
    G_COMM_FLAGS &= (uint16_t)~COMM_TX_OK;
    G_HOST_DELAY = 0x001Eu;

    G_STATUS_FLAGS &= (uint16_t)~STATUS_OUTPUT_ON;
    G_STATUS_FLAGS &= (uint16_t)~STATUS_LED_OR_RELAY;
    G_STATUS_FLAGS &= (uint16_t)~STATUS_RUN_REQUEST;
    G_STATUS_FLAGS_HI &= (uint8_t)~0x01u;
    G_STATUS_FLAGS &= (uint16_t)~STATUS_INHIBIT;
    G_STATUS_FLAGS |= STATUS_INIT_DONE;
    G_STATUS_FLAGS |= STATUS_PWM_STOPPED;
    G_STATUS_FLAGS |= STATUS_CHANGED;

    G_HOST_LIMIT = 0x0080u;
    G_FAULT_FLAGS |= PL30_BIT(0);
    G_FAULT_FLAGS &= (uint16_t)~PL30_BIT(1);
    G_FAULT_FLAGS &= (uint16_t)~PL30_BIT(6);
    G_FAULT_FLAGS |= PL30_BIT(2);
    G_FAULT_FLAGS |= PL30_BIT(4);
    G_FAULT_FLAGS &= (uint16_t)~PL30_BIT(3);

    wait_ticks_0x1D34(3u);
}

static void filter_reset_at(uint16_t descriptor_addr)
{
    /* Equivalent of helper 0x0532: clears descriptor+4 and the pointed history area. */
    uint16_t p = G16((uint16_t)(descriptor_addr + 2u));
    G16((uint16_t)(descriptor_addr + 4u)) = 0u;
    if (p != 0u) {
        G16(p) = 0u;
        G16((uint16_t)(p + 2u)) = 0u;
        G16((uint16_t)(p + 4u)) = 0u;
    }
}

static void init_filters_and_pwm_defaults_0x05EA(void)
{
    /* Filter descriptors from 0x0548 and 0x0578. */
    G_FILT1_PTR = 0x0806u;
    G_FILT1_END = 0x0FF8u;
    filter_reset_at(0x0C32u);
    G16(G_FILT1_PTR + 0u) = 0x428Fu;
    G16(G_FILT1_PTR + 2u) = 0xC666u;
    G16(G_FILT1_PTR + 4u) = 0u;
    G_FILT1_ACC = 0u;

    G_FILT0_PTR = 0x0800u;
    G_FILT0_END = 0x0FF2u;
    filter_reset_at(0x0C28u);
    G16(G_FILT0_PTR + 0u) = 0x7EB7u;
    G16(G_FILT0_PTR + 2u) = 0x87AEu;
    G16(G_FILT0_PTR + 4u) = 0u;
    G_FILT0_ACC = 0u;

    /* PWM1 default configuration from 0x05A4. */
    IOCON1_HIGH &= (uint8_t)~0xE0u;
    IOCON1_HIGH |= 0x0Cu;
    PWMCON1_HIGH = (uint8_t)((PWMCON1_HIGH & 0x3Fu) | 0x80u);
    PWMCON1 &= (uint16_t)~PL30_BIT(1);
    PWMCON1 |= PL30_BIT(0);
    PWMCON1_HIGH |= 0x02u;
    FCLCON1_LOW |= 0x03u;
    G8(0x0435u) &= 0x0Fu;
    G8(0x0434u) &= 0xC0u;

    PHASE1 = 0x19D5u;
    PDC1 = 0u;
    TRIG1 = (uint16_t)(PDC1 >> 1);

    G_CONTROL_FLAGS &= (uint16_t)~CTRL_PWM_ACTIVE;
    G_CONTROL_FLAGS &= (uint16_t)~PL30_BIT(1);
    G_CONTROL_FLAGS &= (uint16_t)~PL30_BIT(3);
    G_PWM_PERIOD_REF = 0x6E20u;
    G_CONTROL_FLAGS &= (uint16_t)~CTRL_RANGE_SELECT;
    G_CONTROL_FLAGS2 &= (uint16_t)~PL30_BIT(7);
}

static void init_adc_0x220C(void)
{
    ADPCFG = 0u;                /* all implemented AN pins analog */
    G8(0x0300u) = (uint8_t)((G8(0x0300u) & 0xF8u) | 0x02u);

    /* Pair-control trigger source setup. Constants follow byte-level writes at 0x220E..0x224C. */
    G8(0x030Au) = (uint8_t)((G8(0x030Au) & 0xE0u) | 0x04u);
    G8(0x030Bu) = (uint8_t)((G8(0x030Bu) & 0xE0u) | 0x04u);
    G8(0x030Cu) = (uint8_t)((G8(0x030Cu) & 0xE0u) | 0x0Cu);
    G8(0x030Du) = (uint8_t)((G8(0x030Du) & 0xE0u) | 0x04u);

    ADSTAT &= (uint16_t)~0x000Fu;
    ADCPC0 |= PL30_BIT(7);
    G8(0x030Bu) |= 0x80u;
    ADCPC1 |= PL30_BIT(7);
    G8(0x030Du) |= 0x80u;

    /* Enable ADC-related interrupts in the original priority/flag registers. */
    G8(0x0091u) &= (uint8_t)~0x40u;
    G8(0x00DBu) = (uint8_t)((G8(0x00DBu) & 0xF8u) | 0x05u);
    G8(0x00A1u) |= 0x40u;
    G8(0x0091u) &= (uint8_t)~0x80u;
    G8(0x00DBu) = (uint8_t)((G8(0x00DBu) & 0x8Fu) | 0x50u);
    G8(0x00A1u) |= 0x80u;
    G8(0x0092u) &= (uint8_t)~0x01u;
    G8(0x00DCu) = (uint8_t)((G8(0x00DCu) & 0xF8u) | 0x04u);
    G8(0x00A2u) |= 0x01u;
    G8(0x0092u) &= (uint8_t)~0x02u;
    G8(0x00DCu) = (uint8_t)((G8(0x00DCu) & 0x8Fu) | 0x50u);
    G8(0x00A2u) |= 0x02u;
}

static void init_comparator1_0x21B6(void)
{
    G8(0x0540u) |= 0xC0u;
    CMPCON1 &= (uint16_t)~PL30_BIT(5);
    CMPCON1 |= PL30_BIT(0);
    CMPDAC1 = 0x03FFu;
    IFS1 &= (uint16_t)~PL30_BIT(2);
    IEC1 |= PL30_BIT(2);
    G8(0x00ADu) = (uint8_t)((G8(0x00ADu) & 0xF8u) | 0x06u);
    G8(0x0541u) |= 0x80u;
}

static void send_status_frame_0x1014(void)
{
    if ((G_COMM_FLAGS & COMM_NEED_RESET) != 0u) {
        G_COMM_FLAGS &= (uint16_t)~COMM_NEED_RESET;
        G_UART_STATE = 1u;
    }

    G_COMM_FLAGS |= COMM_TX_STATUS;
    G_COMM_FLAGS &= (uint16_t)~COMM_TX_OK;
    G_COMM_FLAGS &= (uint16_t)~COMM_TX_RETRY;

    G_UART_TX_STATUS = G_STATUS_FLAGS;
    uint16_t sum = (uint16_t)(((G8(0x0C07u) + G8(0x0C06u)) & 0x00FFu));
    G_UART_TX_CHECKSUM = (uint16_t)((0x0100u - sum) & 0x00FFu);

    uart_send_byte(0xEAu);
    G_UART_IDLE_COUNTER = 0u;
    G_UART_TX_STATE = 1u;
}

static void host_write_handler_0x1060(void)
{
    /* The command byte is a byte offset into a 16-bit register window starting at 0xBC8. */
    uint16_t dst = (uint16_t)(0x0BC8u + ((G_UART_ADDR >> 1) * 2u));
    G16(dst) = G_UART_DATA_LO;

    if (G_UART_ADDR == 0x20u) {
        status_side_effects_0x2374();
    } else if ((G_UART_ADDR == 0x28u) && ((G_CONTROL_FLAGS & CTRL_INPUT_OK) != 0u)) {
        /* Converts host setpoint BF0 into delay/blanking value BF2; three piecewise slopes. */
        uint16_t v = G_HOST_SETPOINT;
        if (v > 0x0DFFu) {
            uint16_t t = (uint16_t)((v * 5u) >> 9);
            if (t <= 0x005Eu) G_HOST_DELAY = (uint16_t)(0x005Fu - t);
        } else if (v > 0x0832u) {
            uint16_t t = (uint16_t)((v * 5u) >> 8);
            if (t <= 0x007Fu) G_HOST_DELAY = (uint16_t)(0x0080u - t);
        } else {
            uint16_t t = (uint16_t)((v * 5u) >> 7);
            if (t <= 0x00A8u) G_HOST_DELAY = (uint16_t)(0x00A9u - t);
        }
        if (G_HOST_DELAY <= 0x001Du) G_HOST_DELAY = 0x001Eu;
    }
}

static void control_update_0x09AA(void)
{
    if ((G_STATUS_FLAGS & STATUS_REMOTE_MODE) != 0u) {
        if (G_LINE_LOCK_COUNTER > 7u) G_STATUS_FLAGS_HI &= (uint8_t)~0x01u;
    } else if ((G_ADC_AVG > 0x0063u) && (G_LINE_LOCK_COUNTER == 0u)) {
        G_STATUS_FLAGS_HI |= 0x01u;
    }

    if ((G_STATUS_FLAGS & STATUS_RUN_REQUEST) != 0u) {
        if ((((uint32_t)G_ENERGY_HI << 16) | G_ENERGY_LO) <= 0x00001728ul) {
            G_STATUS_FLAGS &= (uint16_t)~STATUS_RUN_REQUEST;
            G_STATUS_FLAGS &= (uint16_t)~STATUS_FAULT_LATCH;
            G_STATUS_FLAGS |= STATUS_CHANGED;
            G_RUN_TIMER = 0u;
            G_CONTROL_FLAGS &= (uint16_t)~CTRL_INPUT_OK;
            G_CONTROL_FLAGS2 &= (uint16_t)~PL30_BIT(1);
        }
    } else {
        if ((((uint32_t)G_ENERGY_HI << 16) | G_ENERGY_LO) > 0x00001C39ul) {
            G_STATUS_FLAGS |= STATUS_RUN_REQUEST;
            G_STATUS_FLAGS |= STATUS_CHANGED;
            G_LINE_LOCK_COUNTER = 0u;
        }
    }

    bool drive_gate = false;
    if (((G_STATUS_FLAGS & STATUS_REMOTE_MODE) != 0u) &&
        ((G_STATUS_FLAGS & STATUS_RUN_REQUEST) != 0u) &&
        ((G_CONTROL_FLAGS & PL30_BIT(10)) == 0u)) {
        drive_gate = true;
    }
    if (drive_gate) G8(0x02CDu) |= 0x08u; else G8(0x02CDu) &= (uint8_t)~0x08u;

    if ((G_ADC_DELTA_NOW > 0x00EFu) && (G_ADC_DELTA_1 > 0x00EFu)) {
        G_FAULT_DELAY = 0u;
    }
}

static void line_target_update_0x0A5E(void)
{
    uint16_t startup_threshold = (uint16_t)((((G_PWM_PERIOD_REF >> 5) * 15u) >> 4));
    if ((G_LINE_ESTIMATE <= startup_threshold) &&
        ((G_CONTROL_FLAGS & CTRL_PERIOD_REACHED) == 0u) &&
        (G_STATE == 2u) &&
        ((G_CONTROL_FLAGS & CTRL_PWM_ACTIVE) != 0u)) {
        G_CONTROL_FLAGS |= CTRL_PRERUN;
    } else if ((G_LINE_ESTIMATE > 0x0293u) ||
               ((G_STATE == 4u) && (G_FAULT_DELAY <= G_HOST_DELAY)) ||
               ((G_FAULT_FLAGS & PL30_BIT(1)) == 0u)) {
        G_CONTROL_FLAGS &= (uint16_t)~CTRL_PRERUN;
        G_RUN_COUNT = 0u;
        if ((G_STATUS_FLAGS & STATUS_OUTPUT_ON) != 0u) {
            G_STATUS_FLAGS &= (uint16_t)~STATUS_OUTPUT_ON;
            G8(0x02CDu) &= (uint8_t)~0x01u;
            G_STATUS_FLAGS |= STATUS_CHANGED;
        }
        if ((G_LINE_ESTIMATE <= 0x012Bu) && ((G_STATUS_FLAGS & STATUS_LED_OR_RELAY) != 0u)) {
            G_STATUS_FLAGS &= (uint16_t)~STATUS_LED_OR_RELAY;
            LATB &= (uint16_t)~PL30_BIT(5);
            G_STATUS_FLAGS |= STATUS_CHANGED;
        }
    }

    if (G_RUN_COUNT > 0x015Du && ((G_STATUS_FLAGS & STATUS_OUTPUT_ON) == 0u)) {
        G_STATUS_FLAGS |= STATUS_OUTPUT_ON | STATUS_LED_OR_RELAY | STATUS_CHANGED;
        LATB |= PL30_BIT(5);
    }

    G_RETRY_OR_BLINK_LIMIT = ((G_STATUS_FLAGS & STATUS_OUTPUT_ON) != 0u) ? 5u : 200u;

    if (((G_STATUS_FLAGS & STATUS_INHIBIT) == 0u) && ((G_CONTROL_FLAGS2 & PL30_BIT(0)) != 0u)) {
        G_STATUS_FLAGS |= STATUS_INHIBIT | STATUS_CHANGED;
    }
}

static void update_sine_envelope_from_main_0x1AFE(void)
{
    G_IDX_B = (uint16_t)(((uint32_t)G_LINE_COUNTER * (uint32_t)G_PHASE_MULT) >> 10);
    G_IDX_A = wrap_table_index_373((uint16_t)((G_IDX_B >> 1) + 100u));
    G_IDX_B = wrap_table_index_373((uint16_t)(G_IDX_B + 260u));
    G_ENV_A = scaled_sine(G_IDX_A, G_ENVELOPE_SCALE);
    G_ENV_B = scaled_sine(G_IDX_B, G_ENVELOPE_SCALE);
}

static void state_machine_from_main_0x1B7A(void)
{
    switch (G_STATE) {
    case 1u: /* pre-start / wait-for-input */
        if ((G_CONTROL_FLAGS & CTRL_INPUT_OK) != 0u) {
            G_STATE = 2u;
        } else if ((G_RUN_TIMER <= 9u) && ((G_FAULT_FLAGS & PL30_BIT(6)) != 0u)) {
            LATB &= (uint16_t)~PL30_BIT(3);
        }
        break;

    case 2u: /* starting */
        if ((G_STATUS_FLAGS & STATUS_RUN_REQUEST) == 0u) {
            if ((G_CONTROL_FLAGS & CTRL_PWM_ACTIVE) != 0u) stop_pwm_soft_0x064A();
            if ((G_FAULT_FLAGS & PL30_BIT(6)) != 0u) LATB &= (uint16_t)~PL30_BIT(3);
            G_STATE = 1u;
        } else if ((G_CONTROL_FLAGS & CTRL_PWM_ACTIVE) == 0u) {
            if (((G_FAULT_FLAGS & PL30_BIT(1)) == 0u) &&
                ((G_STATUS_FLAGS & STATUS_INHIBIT) == 0u) &&
                ((G_CONTROL_FLAGS & CTRL_INPUT_OK) != 0u)) {
                start_pwm_soft_0x0604();
            }
        } else if (((G_FAULT_FLAGS & PL30_BIT(1)) != 0u) ||
                   ((G_STATUS_FLAGS & STATUS_INHIBIT) != 0u)) {
            stop_pwm_soft_0x064A();
        } else {
            pwm_control_active_0x0CEE();
            if ((G_STATUS_FLAGS & STATUS_OUTPUT_ON) != 0u) G_STATE = 3u;
        }
        break;

    case 3u: /* running */
        if (((G_STATUS_FLAGS & STATUS_REMOTE_MODE) == 0u) ||
            (((G_STATUS_FLAGS & STATUS_RUN_REQUEST) == 0u) && ((G_FAULT_FLAGS & PL30_BIT(6)) == 0u))) {
            G_FAULT_DELAY = 0u;
            G_STATE = 4u;
        } else if (((G_STATUS_FLAGS & STATUS_INHIBIT) != 0u) ||
                   ((G_STATUS_FLAGS & STATUS_OUTPUT_ON) == 0u) ||
                   ((G_FAULT_FLAGS & PL30_BIT(1)) != 0u) ||
                   ((G_STATUS_FLAGS & STATUS_RUN_REQUEST) == 0u)) {
            stop_pwm_soft_0x064A();
            if ((G_FAULT_FLAGS & PL30_BIT(6)) != 0u) LATB &= (uint16_t)~PL30_BIT(3);
            G_STATE = 2u;
        } else {
            pwm_control_active_0x0CEE();
            if (((G_OUTPUT_GATE_FLAGS & PL30_BIT(2)) != 0u) &&
                ((G_FAULT_FLAGS & (PL30_BIT(3) | PL30_BIT(5))) == 0u)) {
                G8(0x02CDu) |= 0x01u;
            }
        }
        break;

    case 4u: /* coast/shutdown/retry */
        if (((G_STATUS_FLAGS & STATUS_REMOTE_MODE) != 0u) &&
            ((G_STATUS_FLAGS & STATUS_RUN_REQUEST) != 0u)) {
            G_STATE = 3u;
        } else if (((G_STATUS_FLAGS & STATUS_OUTPUT_ON) == 0u) ||
                   ((G_FAULT_FLAGS & PL30_BIT(1)) != 0u)) {
            stop_pwm_soft_0x064A();
            if ((G_FAULT_FLAGS & PL30_BIT(6)) != 0u) LATB &= (uint16_t)~PL30_BIT(3);
            G_STATE = 1u;
        } else if ((G_STATUS_FLAGS & STATUS_INHIBIT) != 0u) {
            stop_pwm_soft_0x064A();
        } else if ((G_LAST_DUTY_SCALE > (uint16_t)(G_LINE_ESTIMATE + 150u)) ||
                   (((G_CONTROL_FLAGS2 & PL30_BIT(1)) != 0u) && ((G_STATUS_FLAGS & STATUS_OUTPUT_ON) != 0u))) {
            LATB &= (uint16_t)~PL30_BIT(3);
        } else if ((G_FAULT_FLAGS & PL30_BIT(6)) != 0u) {
            LATB &= (uint16_t)~PL30_BIT(3);
        }
        break;

    default:
        G_STATE = 1u;
        G_STATUS_FLAGS &= (uint16_t)~(STATUS_RUN_REQUEST | STATUS_OUTPUT_ON | STATUS_LED_OR_RELAY);
        LATB &= (uint16_t)~PL30_BIT(5);
        break;
    }
}

static void start_pwm_soft_0x0604(void)
{
    if (G_LINE_ESTIMATE > 10u) {
        G_PWM_PERIOD_ACTIVE = (uint16_t)((G_LINE_ESTIMATE << 5) - 0x0140u);
    }
    G_FILT0_ACC = G_PWM_PERIOD_ACTIVE;
    G_SOFTSTART_DUTY = 0u;
    G_FILT1_ACC = 0u;
    filter_reset_at(0x0C28u);
    filter_reset_at(0x0C32u);
    IOCON1_HIGH |= 0x80u;
    G_CONTROL_FLAGS |= CTRL_PWM_ACTIVE;
    G_CONTROL_FLAGS2 |= PL30_BIT(2);
    G_CONTROL_FLAGS |= CTRL_PERIOD_REACHED;
    G_PERIOD_ADJ_COUNTER = 0u;
    if ((G_STATUS_FLAGS & STATUS_PWM_STOPPED) != 0u) {
        G_STATUS_FLAGS &= (uint16_t)~STATUS_PWM_STOPPED;
        G_STATUS_FLAGS |= STATUS_CHANGED;
    }
    G_CONTROL_FLAGS2 |= PL30_BIT(7);
}

static void stop_pwm_soft_0x064A(void)
{
    G_CONTROL_FLAGS2 &= (uint16_t)~PL30_BIT(7);
    G_SOFTSTART_DUTY = 0u;
    G_PWM_PERIOD_ACTIVE = 0u;
    PDC1 = 0u;
    IOCON1_HIGH &= (uint8_t)~0x80u;
    G_RUN_COUNT = 0u;
    G_CONTROL_FLAGS &= (uint16_t)~CTRL_PWM_ACTIVE;
    G_CONTROL_FLAGS2 &= (uint16_t)~PL30_BIT(2);
    G_CONTROL_FLAGS &= (uint16_t)~CTRL_PERIOD_REACHED;
    if ((G_STATUS_FLAGS & STATUS_PWM_STOPPED) == 0u) {
        G_STATUS_FLAGS |= STATUS_PWM_STOPPED | STATUS_CHANGED;
    }
}

static void pwm_control_active_0x0CEE(void)
{
    /* Condensed reconstruction of the large active-control routine.
     * It manages filter coefficients, period target G_PWM_PERIOD_REF, current-limit DAC,
     * and PWM enable/disable around IOCON1 high byte bit 7.
     */
    if ((G_CONTROL_FLAGS & CTRL_PWM_ACTIVE) == 0u) return;

    if (((G_SLOW_TICK_FLAG == 1u) || (G_BURST_MODE == 1u))) {
        if ((G_CONTROL_FLAGS & CTRL_RANGE_SELECT) == 0u) {
            G16(G_FILT1_PTR + 0u) = 0x428Fu;
            G16(G_FILT1_PTR + 2u) = 0xC666u;
            G16(G_FILT1_PTR + 4u) = 0u;
        } else {
            G16(G_FILT1_PTR + 0u) = 0x5D70u;
            G16(G_FILT1_PTR + 2u) = 0xB333u;
            G16(G_FILT1_PTR + 4u) = 0u;
        }
    }

    if ((G_POWER_TARGET > 0x0159u && G_POWER_TARGET <= 0x0177u) ||
        (G_POWER_TARGET > 0x02C1u && G_POWER_TARGET <= 0x02DFu)) {
        G_CONTROL_FLAGS |= CTRL_LIMIT_WINDOW;
    } else if ((G_POWER_TARGET <= 0x0144u) ||
               (G_POWER_TARGET > 0x018Cu && G_POWER_TARGET <= 0x02ACu) ||
               (G_POWER_TARGET > 0x02F4u)) {
        G_CONTROL_FLAGS &= (uint16_t)~CTRL_LIMIT_WINDOW;
    }

    G_PWM_PERIOD_CODE = G_HOST_LIMIT;
    if (G_PWM_PERIOD_CODE > 0x0094u) G_PWM_PERIOD_CODE = 0x0094u;
    if (G_PWM_PERIOD_CODE <= 0x006Bu) G_PWM_PERIOD_CODE = 0x006Cu;

    /* Period targets selected by operating-region flags; these constants are exact. */
    if (((G_CONTROL_FLAGS & PL30_BIT(7)) != 0u) && ((G_CONTROL_FLAGS & PL30_BIT(8)) != 0u)) {
        G_PWM_PERIOD_TARGET = (uint16_t)(0x5560u + (G_PWM_PERIOD_CODE << 5));
    } else if (((G_CONTROL_FLAGS & PL30_BIT(7)) != 0u) && ((G_CONTROL_FLAGS & PL30_BIT(11)) != 0u)) {
        G_PWM_PERIOD_TARGET = (uint16_t)(0x5480u + (G_PWM_PERIOD_CODE << 5));
    } else if ((G_CONTROL_FLAGS & PL30_BIT(9)) != 0u) {
        G_PWM_PERIOD_TARGET = (uint16_t)(0x5F80u + (G_PWM_PERIOD_CODE << 5));
    } else {
        G_PWM_PERIOD_TARGET = (uint16_t)(0x5E20u + (G_PWM_PERIOD_CODE << 5));
    }

    if (G_PWM_PERIOD_TARGET > G_PWM_PERIOD_REF) {
        if (G_PERIOD_ADJ_COUNTER > 4u) { G_PERIOD_ADJ_COUNTER = 0u; G_PWM_PERIOD_REF++; }
    } else if (G_PWM_PERIOD_TARGET < G_PWM_PERIOD_REF) {
        if (G_PERIOD_ADJ_COUNTER > 4u) { G_PERIOD_ADJ_COUNTER = 0u; G_PWM_PERIOD_REF--; }
    }

    if ((IOCON1_HIGH & 0x80u) == 0u) {
        IOCON1_HIGH |= 0x80u;
        G_PWM_PERIOD_ACTIVE = 0x5860u;
    }

    G_PERIOD_LOW_LIMIT = (uint16_t)((G_PWM_PERIOD_REF >> 5) - 35u);

    if (G_STATE == 2u) {
        if (G_PWM_PERIOD_ACTIVE >= G_PWM_PERIOD_REF) {
            G_FILT0_ACC = G_PWM_PERIOD_ACTIVE;
            G_CONTROL_FLAGS |= CTRL_PWM_ACTIVE;
        } else {
            G_FILT0_ACC = G_PWM_PERIOD_REF;
            G_CONTROL_FLAGS &= (uint16_t)~CTRL_PWM_ACTIVE;
        }
    } else {
        G_FILT0_ACC = G_PWM_PERIOD_REF;
        G_CONTROL_FLAGS &= (uint16_t)~CTRL_PWM_ACTIVE;
    }
}

static void housekeeping_0x0B14(void)
{
    if ((G_EVENT_LATCH != 0u) && (G_EVENT_TIMER > 1u)) {
        G_EVENT_TIMER = 2u;
        G_EVENT_LATCH = 0u;
        G_EVENT_ARM = 1u;
    }
    if ((G_EVENT_ARM == 0u) && (G_PERIOD_COUNTER <= 0x24u)) return;
    if ((G_EVENT_ARM != 0u) && (G_PERIOD_COUNTER <= 3u)) return;

    if (G_PERIOD_COUNTER != 0u) {
        G16(0x0BD2u) = (uint16_t)(0x05DCu / G_PERIOD_COUNTER);
    }

    G_AVG_LEVEL = (uint16_t)(G_AVG_LEVEL + (G_AVG_ACCUM >> 2) - (G_AVG_LEVEL >> 2));
    G_AVG_ACCUM = 0u;
    G_PHASE_MULT = (uint16_t)(((uint32_t)G_AVG_LEVEL * 0x0BA800u) >> 16); /* approximation of helper divide */

    if ((PORTB & PL30_BIT(3)) == 0u && (G_STATUS_FLAGS & STATUS_RUN_REQUEST) != 0u &&
        G_RUN_TIMER > G_RETRY_OR_BLINK_LIMIT && (G_LINE_ESTIMATE + 25u) > G_ADC_AVG) {
        LATB |= PL30_BIT(3);
    }

    if (G_POWER_TARGET > 0x01F5u && ((G_CONTROL_FLAGS & CTRL_RANGE_SELECT) == 0u)) {
        G_CONTROL_FLAGS |= CTRL_RANGE_SELECT | CTRL_FAST_LIMIT;
        G_CONTROL_FLAGS2 |= PL30_BIT(6);
    } else if (G_POWER_TARGET <= 0x01D5u && ((G_CONTROL_FLAGS & CTRL_RANGE_SELECT) != 0u)) {
        G_CONTROL_FLAGS &= (uint16_t)~CTRL_RANGE_SELECT;
        G_CONTROL_FLAGS |= CTRL_FAST_LIMIT;
        G_CONTROL_FLAGS2 &= (uint16_t)~PL30_BIT(6);
        CMPDAC1 = 0x03FFu;
    }

    /* Duty target and DAC threshold update, condensed. */
    G_ENVELOPE_SCALE = (uint16_t)(G_ENVELOPE_SCALE - (G_ENVELOPE_SCALE >> 1) + (G_POWER_TARGET >> 1));
    G_DUTY_LIMIT = ((G_ENVELOPE_SCALE <= (uint16_t)(G_POWER_FILT + 0x28u)) ||
                    (G_STATE != 4u) || ((G_STATUS_FLAGS & STATUS_REMOTE_MODE) != 0u))
                    ? G_ENVELOPE_SCALE : G_POWER_FILT;
    G_DUTY_LIMIT_2 = G_ENVELOPE_SCALE;
    G_DUTY_MARGIN = (uint16_t)(((G_ENVELOPE_SCALE * 3u) >> 5) - 0x18u);
    if ((G_CONTROL_FLAGS & CTRL_RANGE_SELECT) == 0u) CMPDAC1 = 0x03FFu; else CMPDAC1 = 0x0294u;
    if ((G_CONTROL_FLAGS & CTRL_INPUT_OK) != 0u) G_LAST_DUTY_SCALE = G_ENVELOPE_SCALE;

    G_EVENT_ARM = 0u;
    G_PERIOD_COUNTER = 0u;
    G_POWER_TARGET = G_PEAK_LATCH;
    G_ADC2_PEAK_COPY = G_ADC2_PEAK;
    G_PEAK_LATCH = 0u;
    G_ADC2_PEAK = 0u;
}

static void math_housekeeping_0x1F5C(void)
{
    /* The original updates rolling RMS/energy estimates using compiler 32-bit math helpers. */
    if (G_RMS_A_LO == 0u && G_RMS_A_HI == 0u) G_RMS_A_LO = 1u;
    if (G_RMS_B_LO == 0u && G_RMS_B_HI == 0u) G_RMS_B_LO = 1u;
    G16(0x0BCCu) = G_RMS_A_LO;
    G16(0x0BD0u) = G_BC0_LO;
    G16(0x0BCAu) = (uint16_t)(((uint32_t)G_RMS_B_LO * 0x00E7u) >> 4);
    G16(0x0BE6u) = G_ADC5_SAMPLE;
}

static void status_side_effects_0x2374(void)
{
    if (((G_FAULT_FLAGS & PL30_BIT(2)) != 0u) && ((G_OUTPUT_GATE_FLAGS & PL30_BIT(2)) != 0u)) {
        G_OUTPUT_GATE_FLAGS &= (uint16_t)~PL30_BIT(2);
        G8(0x02CDu) &= (uint8_t)~0x01u;
    } else if (((G_FAULT_FLAGS & PL30_BIT(2)) == 0u) && ((G_OUTPUT_GATE_FLAGS & PL30_BIT(2)) == 0u)) {
        G_OUTPUT_GATE_FLAGS |= PL30_BIT(2);
    }

    if (((G_FAULT_FLAGS & PL30_BIT(3)) != 0u) || ((G_FAULT_FLAGS & PL30_BIT(5)) != 0u)) {
        G8(0x02CDu) &= (uint8_t)~0x01u;
    }
}

/* Interrupt reconstructions. Vector names need verifying against the XC16 header.  The bodies are
 * kept callable with neutral names to avoid binding the wrong vector accidentally.
 */
void pl30_timer1_isr_body_0x1D4A(void)
{
    if ((G_CONTROL_FLAGS & CTRL_RANGE_SELECT) == 0u) {
        G_INPUT_THRESHOLD = 0x00ACu;
        G_INPUT_DEBOUNCE_LIMIT = 0x0014u;
        G_LINE_LOCK_LIMIT = 0x000Bu;
    } else {
        G_INPUT_THRESHOLD = 0x00BEu;
        G_INPUT_DEBOUNCE_LIMIT = 0x000Fu;
        G_LINE_LOCK_LIMIT = 0x0021u;
    }

    if (G_ADC_AVG < G_INPUT_THRESHOLD) {
        if (++G_THRESH_HI_COUNT > G_INPUT_DEBOUNCE_LIMIT) {
            G_CONTROL_FLAGS_HI |= 0x04u;
            G_THRESH_LO_COUNT = 0u;
        }
    } else if (G_ADC_AVG <= (uint16_t)(G_INPUT_THRESHOLD + 0x10u)) {
        G_THRESH_HI_COUNT = 0u;
        if (G_THRESH_LO_COUNT > 0x16u) G_CONTROL_FLAGS_HI &= (uint8_t)~0x04u;
    }

    if (++G_DELAY_SUBTICKS > 2u) {
        G_DELAY_SUBTICKS = 0u;
        G_DELAY_TICKS++;
        if (++G_COMP_REENABLE_DELAY > 10u) IEC1 |= PL30_BIT(2);

        if ((G_STATUS_FLAGS & STATUS_RUN_REQUEST) != 0u) {
            G_THRESH_LO_COUNT++;
            G_RUN_TIMER++;
            if (G_RUN_TIMER > 29u && (PORTB & PL30_BIT(3)) != 0u) {
                G_CONTROL_FLAGS |= CTRL_INPUT_OK;
                G_CONTROL_FLAGS2 |= PL30_BIT(1);
            }
        }

        G_PERIOD_COUNTER++;
        if ((G_CONTROL_FLAGS & CTRL_PERIOD_REACHED) != 0u) {
            if (G_RAMP_STEP > 80u) G_RAMP_STEP = 80u;
            if (G_RAMP_STEP < 3u) G_RAMP_STEP = 3u;
            if (G_PWM_PERIOD_ACTIVE <= 0x7FFEu) G_PWM_PERIOD_ACTIVE++;
        }
    }

    IFS0 &= (uint16_t)~PL30_BIT(3);
}

void pl30_adc_pair0_isr_body_0x14C0(void)
{
    uint16_t a = ADCBUF0;
    uint16_t b = ADCBUF1;
    uint16_t d = (a >= b) ? (uint16_t)(a - b) : (uint16_t)(b - a);

    G_ADC_DELTA_1 = G_ADC_DELTA_NOW;
    G_ADC_DELTA_NOW = d;
    /* 8-sample moving average over B14..B2C approximates original shift register. */
    G_ADC_AVG = (uint16_t)((G_ADC_AVG * 7u + d) >> 3);
    G_LINE_COUNTER++;
    G_AVG_ACCUM++;
    if (d > G_PEAK_LATCH) { G_PEAK_LATCH = d; G_CONTROL_FLAGS2 |= PL30_BIT(5); }
    else { G_CONTROL_FLAGS2 &= (uint16_t)~PL30_BIT(5); }

    if ((G_ADC_DELTA_1 < 0x11u) && (G_ADC_DELTA_NOW > 0x11u)) {
        G_EVENT_LATCH |= 1u;
        G_EVENT_TIMER = 0u;
        G_LINE_COUNTER = 0u;
    }

    ADSTAT &= (uint16_t)~PL30_BIT(0);
    G8(0x0091u) &= (uint8_t)~0x40u;
}

void pl30_adc_pair1_isr_body_0x156A(void)
{
    if ((PORTB & PL30_BIT(4)) != 0u) {
        if (G_PORTB4_DEBOUNCE <= 100u) G_PORTB4_DEBOUNCE++;
        if (G_PORTB4_DEBOUNCE >= 3u) G_CONTROL_FLAGS2 |= PL30_BIT(0);
    } else {
        G_PORTB4_DEBOUNCE = 0u;
    }

    if ((G_CONTROL_FLAGS2 & PL30_BIT(2)) != 0u) IOCON1_HIGH |= 0x80u;
    G_ADC3_SAMPLE = ADCBUF3;
    G_ADC2_SAMPLE = ADCBUF2;
    G_B6C = (uint16_t)(G_B6C + G_ADC6_SAMPLE - G_LINE_ESTIMATE);
    G_LINE_ESTIMATE = (uint16_t)(((int16_t)G_B6C) >> 2);

    if (G_LINE_ESTIMATE >= 0x03B6u) G_CONTROL_FLAGS2 |= PL30_BIT(0);
    if ((G_CONTROL_FLAGS2 & PL30_BIT(0)) == 0u) {
        PDC1 = 0u;
        ADSTAT &= (uint16_t)~PL30_BIT(1);
        G8(0x0091u) &= (uint8_t)~0x80u;
        return;
    }

    /* Highly condensed duty calculation. The original clamps with B7E/current limits,
     * table-derived envelope values, soft-start B72, and phase scale B12.
     */
    uint16_t duty = G_ENV_A;
    if (duty > G_DUTY_LIMIT) duty = G_DUTY_LIMIT;
    duty = (uint16_t)(((uint32_t)duty * (uint32_t)G_SCALE_PHASE) >> 14);
    if ((G_CONTROL_FLAGS2 & PL30_BIT(6)) != 0u && G_SOFTSTART_DUTY < duty) duty = G_SOFTSTART_DUTY;
    if (duty < 2u) duty = 2u;
    if (duty > G_CURRENT_LIMIT_CODE) duty = G_CURRENT_LIMIT_CODE;
    PDC1 = (uint16_t)(duty >> 1);
    TRIG1 = (uint16_t)(PDC1 >> 1);

    G_SCALE_PHASE = ((G_MODE_BITS & PL30_BIT(4)) != 0u) ? 0x1A63u : 0x19D5u;
    PHASE1 = G_SCALE_PHASE;

    ADSTAT &= (uint16_t)~PL30_BIT(1);
    G8(0x0091u) &= (uint8_t)~0x80u;
}

void pl30_adc_pair2_isr_body_0x201E(void)
{
    G_ADC4_SAMPLE = ADCBUF4;
    G_ADC5_SAMPLE = ADCBUF5;
    G_ADC4_OFFSET = (uint16_t)(G_ADC_DELTA_NOW + 5u);
    G_ADC4_SAMPLE = (uint16_t)(G_ADC4_SAMPLE + ((G_ADC4_SAMPLE * 13u) >> 6));
    /* Original computes rolling 32-bit square/integral values. */
    ADSTAT &= (uint16_t)~PL30_BIT(2);
    G8(0x0092u) &= (uint8_t)~0x01u;
}

void pl30_adc_pair3_isr_body_0x22AC(void)
{
    G_ADC6_SAMPLE = ADCBUF6;
    if ((G_ADC6_SAMPLE <= 0x0333u) && (G_STATE == 3u) && ((uint16_t)(G_POWER_TARGET + 10u) >= G_ENVELOPE_SCALE)) {
        G_DUTY_LIMIT = (uint16_t)((G_ENVELOPE_SCALE * 5u) >> 3);
    }

    if (G_POWER_TARGET <= 0x026Bu) {
        if (G16(G_FILT1_PTR) == 0x5333u && G16(G_FILT1_PTR + 2u) == 0xC000u && G_ADC3_SAMPLE > 0x50u) {
            G16(G_FILT1_PTR) = 0x428Fu;
            G16(G_FILT1_PTR + 2u) = 0xC666u;
            G16(G_FILT1_PTR + 4u) = 0u;
        }
        if (G_ADC3_SAMPLE > 0x87u) G_MODE_BITS |= PL30_BIT(0);
    } else if (G_POWER_TARGET > 0x0280u) {
        if (G16(G_FILT1_PTR) == 0x5FFFu && G16(G_FILT1_PTR + 2u) == 0xC000u && G_ADC3_SAMPLE > 0x46u) {
            G16(G_FILT1_PTR) = 0x6147u;
            G16(G_FILT1_PTR + 2u) = 0xAE14u;
            G16(G_FILT1_PTR + 4u) = 0u;
        }
    }

    ADSTAT &= (uint16_t)~PL30_BIT(3);
    G8(0x0092u) &= (uint8_t)~0x02u;
}

void pl30_comparator1_isr_body_0x100C(void)
{
    IFS1 &= (uint16_t)~PL30_BIT(2);
}

void pl30_trap_pwm_off_loop(void)
{
    PTCON_HIGH &= (uint8_t)~0x80u;
    while (1) { ; }
}
