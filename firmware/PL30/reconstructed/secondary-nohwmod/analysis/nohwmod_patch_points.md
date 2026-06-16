# NoHWMod editable patch points

## ADC output-voltage scaling helper

Addresses:

- Low helper: `0x5B00..0x5B18`
- High helper: `0xA220..0xA238`
- Low call site: `0x385C`
- High call site: `0x945C`

Editable C equivalent:

```c
#define SEC_NOHWMOD_ADC_SCALE_Q15 0x6AC2u
uint16_t scaled_vout = ((uint32_t)ADCBUF2 * SEC_NOHWMOD_ADC_SCALE_Q15) >> 15;
```

Changing `SEC_NOHWMOD_ADC_SCALE_Q15` changes the apparent ADC feedback sent to the rest of the firmware.

Physical threshold multiplier for a given Q15 scale:

```c
physical_multiplier = 32768.0f / SEC_NOHWMOD_ADC_SCALE_Q15;
```

For the recovered modified image:

- scale count: `0x6AC2` = `27330`
- apparent ADC ratio: `0.834045410`
- physical multiplier: `1.198975485`

## Float literal at 0x39B4 / 0x95B4

Stock literal:

```c
float stock = 25079.80859375f;  // bits 0x46C3EF9E
```

NoHWMod literal:

```c
float nohwmod = 29303.80859375f; // bits 0x46E4EF9E
```

This lives in the output-command/PWM4 computation path around low `0x393A` / high `0x953A`, not in startup tables or configuration bits.

## Do not change blindly

A smaller Q15 scale makes the PSU run to a higher physical voltage before the firmware thinks the target/OVP threshold has been reached. A larger Q15 scale lowers the physical setpoint/trip point. Verify the true ADC divider ratio, output capacitor voltage rating, MOSFET/rectifier stress, transformer insulation margin, and secondary feedback calibration before changing this again.
