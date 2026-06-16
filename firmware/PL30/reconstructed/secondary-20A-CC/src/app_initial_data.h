#ifndef PL30SEC_APP_INITIAL_DATA_H
#define PL30SEC_APP_INITIAL_DATA_H

#include <stdint.h>

/*
 * Human-readable decode of the XC16 C-startup data tables recovered at 0x44E4
 * for the low application image and 0xA0E4 for the high duplicate image.  The
 * arrays below are not automatically copied by this reconstruction; they are
 * included so startup defaults can be searched and edited from C.
 */
static const uint16_t sec_init_0E54[] = { 0x03FFu, 0x0000u, 0x0000u, 0x01F3u, 0x0000u, 0x0000u, 0x0000u, 0x0000u };
static const uint16_t sec_init_0E02[] = {
    0xFFFFu, 0xFFFFu, 0xFFFFu, 0xFFFFu, 0xFFFFu, 0xFFFFu, 0xFFFFu, 0xFFFFu,
    0x0000u, 0x0000u, 0x0000u, 0x0000u, 0x0000u, 0x0000u, 0x0001u, 0x0001u
};
static const uint16_t sec_init_0DD6[] = {
    0x0190u, 0x0190u, 0x0001u, 0x0000u, 0x0000u, 0x0001u, 0x0000u, 0x0000u,
    0x0000u, 0x0000u, 0x0000u, 0x0000u, 0x0000u, 0x0FA0u, 0x0000u, 0x0001u,
    0x0000u, 0x0000u, 0x3133u, 0x0001u, 0x0000u, 0x0C6Cu
};
static const uint16_t sec_init_0E8C[] = { 0x0000u, 0x0001u, 0x0000u, 0x0000u, 0x0000u };
static const uint16_t sec_init_0E96[] = { 0x00B4u, 0x00ACu, 0x008Au, 0x0000u, 0x0000u };
static const uint16_t sec_init_0EA0[] = { 0x0000u, 0x0000u, 0x0000u, 0x1900u, 0xFFFFu };
static const uint16_t sec_init_0E80[] = { 0x002Eu, 0x0000u, 0x0000u, 0x0000u, 0x0006u, 0x0000u };
static const uint16_t sec_init_0E72[] = { 0x0000u, 0x0000u, 0x0000u, 0x0000u, 0x0000u, 0x0000u, 0x0000u };

/* Bootloader startup table recovered at 0x5AA6. */
static const uint16_t sec_boot_init_0C2E[] = { 0x0000u, 0x0000u, 0x0000u, 0x0000u, 0x0000u };
static const uint16_t sec_boot_init_0C38[] = { 0x0000u, 0x0000u, 0x0000u };
static const uint16_t sec_boot_init_0C3E[] = { 0x01F3u };

#endif /* PL30SEC_APP_INITIAL_DATA_H */
