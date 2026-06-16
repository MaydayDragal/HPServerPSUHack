/*
 * PL30 primary-side reverse-engineering symbols for dsPIC33FJ16GS502.
 * Addresses are dsPIC data-memory byte addresses observed in the file-register dump and disassembly.
 * Names are descriptive, not original compiler symbols.
 */
#ifndef PL30_SYMBOLS_H
#define PL30_SYMBOLS_H

#include <stdint.h>
#include <stdbool.h>
#include <xc.h>

#define PL30_BIT(n)              ((uint16_t)(1u << (n)))
#define PL30_BIT8(n)             ((uint8_t)(1u << (n)))
#define PL30_MEM16(addr)         (*(volatile uint16_t *)(uintptr_t)(addr))
#define PL30_MEM8(addr)          (*(volatile uint8_t  *)(uintptr_t)(addr))

/* Raw data-RAM aliases. */
#define G16(addr)                PL30_MEM16(addr)
#define G8(addr)                 PL30_MEM8(addr)

/* Main state machine and sine-envelope values. */
#define G_STATE                  G16(0x0814u)  /* 1..4 state machine */
#define G_IDX_A                  G16(0x0B00u)  /* sine index A */
#define G_IDX_B                  G16(0x0B02u)  /* sine index B */
#define G_ENV_A                  G16(0x0B04u)  /* scaled table result A */
#define G_ENV_B                  G16(0x0B06u)  /* scaled table result B */
#define G_FILTER_A               G16(0x0B08u)
#define G_FILTER_B               G16(0x0B0Au)
#define G_SCALE_PHASE            G16(0x0B12u)  /* also written to PHASE1 */
#define G_ADC_DELTA_NOW          G16(0x0B14u)
#define G_ADC_DELTA_1            G16(0x0B16u)
#define G_ADC_AVG                G16(0x0B2Eu)
#define G_LINE_COUNTER           G16(0x0B30u)
#define G_AVG_ACCUM              G16(0x0B32u)
#define G_AVG_LEVEL              G16(0x0B34u)
#define G_PHASE_MULT             G16(0x0B36u)
#define G_ADC2_SAMPLE            G16(0x0B38u)
#define G_MODE_BITS              G16(0x0B3Au)
#define G_EVENT_LATCH            G16(0x0B3Eu)
#define G_EVENT_ARM              G16(0x0B40u)
#define G_EVENT_TIMER            G16(0x0B42u)
#define G_RUN_TIMER              G16(0x0B44u)
#define G_RETRY_OR_BLINK_LIMIT   G16(0x0B4Au)
#define G_POWER_FILT             G16(0x0B4Cu)
#define G_PEAK_LATCH             G16(0x0B4Eu)
#define G_POWER_TARGET           G16(0x0B50u)
#define G_ENVELOPE_SCALE         G16(0x0B52u)
#define G_DUTY_LIMIT             G16(0x0B54u)
#define G_DUTY_LIMIT_2           G16(0x0B56u)
#define G_ADC2_PEAK              G16(0x0B58u)
#define G_ADC2_PEAK_COPY         G16(0x0B5Au)
#define G_DUTY_MARGIN            G16(0x0B5Eu)
#define G_PERIOD_COUNTER         G16(0x0B60u)
#define G_PORTB4_DEBOUNCE        G16(0x0B62u)
#define G_ADC6_SAMPLE            G16(0x0B6Au)
#define G_B6C                    G16(0x0B6Cu)
#define G_LINE_ESTIMATE          G16(0x0B6Eu)
#define G_RAMP_STEP              G16(0x0B70u)
#define G_SOFTSTART_DUTY         G16(0x0B72u)
#define G_BURST_COUNTER          G16(0x0B74u)
#define G_PWM_PERIOD_CODE        G16(0x0B76u)
#define G_BURST_MODE             G16(0x0B78u)
#define G_PERIOD_LOW_LIMIT       G16(0x0B7Au)
#define G_FAULT_REPEAT           G16(0x0B7Cu)
#define G_CURRENT_LIMIT_CODE     G16(0x0B7Eu)

/* Accumulators / filtered measurements around 0x0B80. */
#define G_ADC4_SAMPLE            G16(0x0B80u)
#define G_ADC4_OFFSET            G16(0x0B82u)
#define G_ADC5_SAMPLE            G16(0x0B84u)
#define G_RMS_A_LO               G16(0x0B94u)
#define G_RMS_A_HI               G16(0x0B96u)
#define G_RMS_B_LO               G16(0x0BA4u)
#define G_RMS_B_HI               G16(0x0BA6u)
#define G_ENERGY_LO              G16(0x0BACu)
#define G_ENERGY_HI              G16(0x0BAEu)
#define G_BC0_LO                 G16(0x0BC0u)
#define G_BC0_HI                 G16(0x0BC2u)
#define G_BC4_LO                 G16(0x0BC4u)
#define G_BC4_HI                 G16(0x0BC6u)

/* Host protocol / flags; protocol starts with 0xEA and ACKs with 0x18. */
#define G_STATUS_FLAGS           G16(0x0BC8u)
#define G_STATUS_FLAGS_HI        G8(0x0BC9u)
#define G_FAULT_FLAGS            G16(0x0BE8u)
#define G_HOST_SETPOINT          G16(0x0BF0u)
#define G_HOST_DELAY             G16(0x0BF2u)
#define G_HOST_LIMIT             G16(0x0BF4u)
#define G_UART_STATE             G16(0x0BF6u)
#define G_UART_RX                G16(0x0BF8u)
#define G_UART_ADDR              G16(0x0BFAu)
#define G_UART_SUM               G16(0x0BFCu)
#define G_UART_REPLY_SUM         G16(0x0BFEu)
#define G_UART_DATA_LO           G16(0x0C00u)
#define G_UART_DATA_HI           G16(0x0C02u)
#define G_UART_TX_STATE          G16(0x0C04u)
#define G_UART_TX_STATUS         G16(0x0C06u)
#define G_UART_TX_CHECKSUM       G16(0x0C08u)
#define G_TICK_0C0A              G16(0x0C0Au)
#define G_COMM_FLAGS             G16(0x0C0Cu)

/* Control loops and timebase. */
#define G_PWM_PERIOD_REF         G16(0x0C0Eu)
#define G_PWM_PERIOD_TARGET      G16(0x0C10u)
#define G_PWM_PERIOD_ACTIVE      G16(0x0C12u)
#define G_RUN_COUNT              G16(0x0C14u)
#define G_FAULT_DELAY            G16(0x0C16u)
#define G_LAST_DUTY_SCALE        G16(0x0C18u)
#define G_COMP_REENABLE_DELAY    G16(0x0C1Au)
#define G_PERIOD_ADJ_COUNTER     G16(0x0C1Cu)
#define G_THRESH_HI_COUNT        G16(0x0C1Eu)
#define G_THRESH_LO_COUNT        G16(0x0C20u)
#define G_INPUT_THRESHOLD        G16(0x0C22u)
#define G_INPUT_DEBOUNCE_LIMIT   G16(0x0C24u)
#define G_CONTROL_FLAGS          G16(0x0C26u)
#define G_CONTROL_FLAGS_HI       G8(0x0C27u)

/* Pointer triples used by compiler-like filter helpers. */
#define G_FILT0_PTR              G16(0x0C28u)
#define G_FILT0_END              G16(0x0C2Au)
#define G_FILT0_ACC              G16(0x0C30u)
#define G_FILT1_PTR              G16(0x0C32u)
#define G_FILT1_END              G16(0x0C34u)
#define G_FILT1_ACC              G16(0x0C3Au)
#define G_ADC3_SAMPLE            G16(0x0C4Eu)
#define G_OUTPUT_GATE_FLAGS      G16(0x0C58u)
#define G_DELAY_TICKS            G16(0x0C64u)
#define G_SLOW_TICK_FLAG         G16(0x0C66u)
#define G_C68                    G16(0x0C68u)
#define G_LINE_LOCK_COUNTER      G16(0x0C6Au)
#define G_C6C                    G16(0x0C6Cu)
#define G_LINE_LOCK_LIMIT        G16(0x0C6Eu)
#define G_DELAY_SUBTICKS         G16(0x0C70u)
#define G_UART_IDLE_COUNTER      G16(0x0C72u)
#define G_TMP_C74                G16(0x0C74u)
#define G_CONTROL_FLAGS2         G16(0x0C76u)

/* Flag masks; names are inferred from behavior, not original source. */
#define STATUS_OUTPUT_ON         PL30_BIT(0)
#define STATUS_RUN_REQUEST       PL30_BIT(1)
#define STATUS_INHIBIT           PL30_BIT(2)
#define STATUS_INIT_DONE         PL30_BIT(3)
#define STATUS_CHANGED           PL30_BIT(4)
#define STATUS_LED_OR_RELAY      PL30_BIT(5)
#define STATUS_FAULT_LATCH       PL30_BIT(6)
#define STATUS_PWM_STOPPED       PL30_BIT(7)
#define STATUS_REMOTE_MODE       PL30_BIT(8)

#define CTRL_INPUT_OK            PL30_BIT(0)
#define CTRL_PRERUN              PL30_BIT(1)
#define CTRL_PWM_ACTIVE          PL30_BIT(2)
#define CTRL_PERIOD_REACHED      PL30_BIT(4)
#define CTRL_RANGE_SELECT        PL30_BIT(5)
#define CTRL_FAST_LIMIT          PL30_BIT(6)
#define CTRL_LIMIT_WINDOW        PL30_BIT(7)

#define COMM_NEED_RESET          PL30_BIT(0)
#define COMM_READ_MODE           PL30_BIT(1)
#define COMM_TX_STATUS           PL30_BIT(2)
#define COMM_TX_OK               PL30_BIT(3)
#define COMM_TX_RETRY            PL30_BIT(4)

static inline void bit_set16(volatile uint16_t *reg, uint16_t mask) { *reg = (uint16_t)(*reg | mask); }
static inline void bit_clr16(volatile uint16_t *reg, uint16_t mask) { *reg = (uint16_t)(*reg & (uint16_t)~mask); }
static inline bool bit_tst16(volatile uint16_t *reg, uint16_t mask) { return ((*reg & mask) != 0u); }

/* Some disassembly uses odd byte addresses for high bytes of 16-bit SFRs. */
#define PTCON_HIGH              G8(0x0401u)
#define PWMCON1_HIGH            G8(0x0421u)
#define IOCON1_HIGH             G8(0x0423u)
#define FCLCON1_LOW             G8(0x0424u)
#define ADSTAT_LOW              G8(0x0306u)
#define ADCON_HIGH              G8(0x0301u)

#endif /* PL30_SYMBOLS_H */
