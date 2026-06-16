/*
 * NoHWMod PL30 Rev.10 secondary-side configuration bits; same values as stock secondary.
 * Target: dsPIC33FJ64GS606.  Verify pragma names against the XC16 device header
 * shipped with your MPLAB/XC16 version before programming hardware.
 */
#include <xc.h>

#pragma config BWRP = WRPROTECT_OFF
#pragma config BSS  = NO_FLASH
#pragma config GWRP = OFF
#pragma config GSS  = OFF
#pragma config FNOSC = FRC
#pragma config IESO = ON
#pragma config POSCMD = NONE
#pragma config OSCIOFNC = ON
#pragma config FCKSM = CSECMD
#pragma config WDTPOST = PS32768
#pragma config WDTPRE = PR128
#pragma config WINDIS = OFF
#pragma config FWDTEN = OFF
#pragma config FPWRT = PWR128
#pragma config ALTSS1 = ON
#pragma config ALTQIO = OFF
#pragma config ICS = PGD2
#pragma config JTAGEN = OFF
#pragma config HYST0 = HYST45
#pragma config CMPPOL0 = POL_FALL
#pragma config HYST1 = HYST45
#pragma config CMPPOL1 = POL_FALL
