/* Original PL30 Rev.10 primary-side dsPIC33FJ16GS502 configuration bits.
 * Taken from: DSPIC33FJ16GS502 Stock PL30 Rev.10 Pri ConfigBits.txt
 */
#include <xc.h>

#pragma config BWRP    = WRPROTECT_OFF  /* Boot segment may be written */
#pragma config BSS     = NO_FLASH        /* No boot program Flash segment */
#pragma config GWRP    = ON              /* General Segment write-protected in stock image */
#pragma config GSS     = OFF             /* User program memory not code-protected */
#pragma config FNOSC   = FRC             /* Initial oscillator: internal FRC */
#pragma config IESO    = ON              /* Two-speed oscillator startup enabled */
#pragma config POSCMD  = NONE            /* Primary oscillator disabled */
#pragma config OSCIOFNC= OFF             /* OSC2 is clock output */
#pragma config IOL1WAY = ON              /* PPS can be configured once */
#pragma config FCKSM   = CSECMD          /* Clock switching enabled; FSCM disabled */
#pragma config WDTPOST = PS32768
#pragma config WDTPRE  = PR128
#pragma config WINDIS  = OFF             /* Non-window WDT */
#pragma config FWDTEN  = OFF             /* WDT controlled by software */
#pragma config FPWRT   = PWR128          /* 128 ms power-up timer */
#pragma config ICS     = PGD1            /* Debug channel PGEC1/PGED1 */
#pragma config JTAGEN  = OFF
