/*
 * PL30 secondary-side boot/update selector reconstruction.
 * Reset vector: 0x0000 -> GOTO 0x5000.  The code below models the bootloader
 * behavior at source level; it is not intended to be linked together with the
 * stock application without an explicit linker script and vector table.
 */
#include <xc.h>
#include <stdint.h>
#include <stdbool.h>
#include "pl30sec_symbols.h"

static void boot_init_clock_0x5A22(void);
static void boot_init_gpio_0x5A62(void);
static void boot_init_timers_0x5A80(void);
static void boot_init_pwm4_0x5A8C(void);
static bool boot_validate_low_app_0x563A(void);
static void boot_service_i2c2_0x50B8(void);
static void boot_service_i2c1_protocol_0x543E(void);
static void boot_program_flash_if_requested_0x5742(void);

#ifndef PL30SEC_BOOT_RECON_HAS_ENTRY
#define PL30SEC_BOOT_RECON_HAS_ENTRY 0
#endif

#if PL30SEC_BOOT_RECON_HAS_ENTRY
void boot_entry_0x5000(void)
{
    boot_init_clock_0x5A22();
    boot_init_gpio_0x5A62();
    boot_init_timers_0x5A80();
    boot_init_pwm4_0x5A8C();

    /* Boot marker is read from program flash around 0x5C00 in the stock image. */
    SEC_BOOT_MARKER_BYTE = SEC_BOOT_MARKER_BYTE;

    if (boot_validate_low_app_0x563A()) {
        ((void (*)(void))0x0400u)();
    }

    /* Fallback image used when low image metadata/checksum is absent or invalid. */
    ((void (*)(void))0x6000u)();

    for (;;) {
        boot_service_i2c2_0x50B8();
        boot_service_i2c1_protocol_0x543E();
        boot_program_flash_if_requested_0x5742();
    }
}
#endif

static void boot_init_clock_0x5A22(void)
{
    SEC_PLLFBD = 0x0029u;
    SEC_ACLKCON = 0xA740u;
    SEC_PTCON2 = 0x0001u;
}

static void boot_init_gpio_0x5A62(void)
{
#if defined(LATB)
    LATB = 0u; TRISB = 0xFFFFu;
    LATC = 0u; TRISC = 0xFFFFu;
    LATD = 0u; TRISD = 0xFFFFu;
    LATE = 0u; TRISE = 0xFFFFu;
    LATF = 0u; TRISF = 0xFFFFu;
    LATG = 0u; TRISG = 0xFFFFu;
#endif
}

static void boot_init_timers_0x5A80(void)
{
#if defined(PR1)
    PR1 = 0x9C40u;
    T1CON |= 0x8000u;
#endif
    SEC_IEC0 |= PL30SEC_BIT(3);
}

static void boot_init_pwm4_0x5A8C(void)
{
    PL30SEC_SET_BIT8(0x0483u, 6);
    SEC_SPHASE4 = 0x4A38u;
    SEC_SDC4 = 0x0514u;
}

static bool boot_validate_low_app_0x563A(void)
{
    /* Stock routine checks metadata around the end of the low image:
     *  - 0x4FFC -> SEC_BOOT_LOW_IMAGE_TAG
     *  - 0x4FFA -> SEC_BOOT_LOW_IMAGE_MARKER, expected 0x003C
     *  - accumulated checksum over 0x0104..0x01FF and 0x0400..0x4FFF must be zero
     * It sets SEC_BOOT_VALID_FLAGS bit1 on success.
     */
    if ((SEC_BOOT_VALID_FLAGS & PL30SEC_BIT(1)) == 0u) {
        return false;
    }
    if (SEC_BOOT_LOW_IMAGE_CHECK == 0u || SEC_BOOT_LOW_IMAGE_CHECK == 0xFFFFu) {
        return false;
    }
    if (SEC_BOOT_LOW_IMAGE_TAG == 0u || SEC_BOOT_LOW_IMAGE_TAG == 0xFFFFu) {
        return false;
    }
    if (SEC_BOOT_LOW_IMAGE_MARKER != 0x003Cu) {
        return false;
    }
    return true;
}

static void boot_service_i2c2_0x50B8(void)
{
    if ((SEC_I2C2_FLAGS & PL30SEC_BIT(0)) == 0u) {
        SEC_I2C2CON = 0x8200u;
        SEC_I2C2BRG = 0x018Bu;
        SEC_I2C2_FLAGS |= PL30SEC_BIT(0);
    }
}

static void boot_service_i2c1_protocol_0x543E(void)
{
    /* Boot-mode I2C1 command parser.  The stock code receives flash chunks into
     * boot workspace RAM and sets SEC_BOOT_ACTION_FLAGS bit0 to request an erase
     * or row program through the NVM helper routines at 0x5774/0x5804/0x58BC.
     */
    if ((SEC_I2C1STAT & 0x0001u) != 0u) {
        volatile uint16_t rx = SEC_I2C1RCV;
        (void)rx;
    }
}

static void boot_program_flash_if_requested_0x5742(void)
{
    if ((SEC_BOOT_ACTION_FLAGS & PL30SEC_BIT(0)) == 0u) {
        return;
    }

    SEC_NVMCON = 0x4003u;       /* placeholder erase/program operation */
    SEC_NVMKEY = 0x0055u;
    SEC_NVMKEY = 0x00AAu;
    SEC_NVMCON |= 0x8000u;
    SEC_BOOT_ACTION_FLAGS &= (uint16_t)~PL30SEC_BIT(0);
}
