# Stock vs NoHWMod secondary firmware diff summary

## Exact binary delta

The modified NoHWMod secondary image differs from the stock secondary image by **30 program-memory words**, grouped as follows:

| Group | Low/high image | What changed |
|---|---:|---|
| `0x385C` | Low app | `MOV ADCBUF2,W1` changed to `RCALL 0x5B00`. |
| `0x39B4` | Low app | Float literal high word changed from `0x46C3` to `0x46E4`. |
| `0x5B00..0x5B18` | Low app blank space | New ADC feedback scaling helper. |
| `0x945C` | High app | Mirrored `MOV ADCBUF2,W1` changed to `RCALL 0xA220`. |
| `0x95B4` | High app | Mirrored float literal high word changed from `0x46C3` to `0x46E4`. |
| `0xA220..0xA238` | High app blank space | New ADC feedback scaling helper. |

No configuration-bit change was found; the NoHWMod config-bit file is byte-identical to the stock secondary config-bit dump.

## Output-voltage / OVP feedback change

Stock low/high ADC ISR behavior:

```asm
385C: MOV ADCBUF2, W1      ; low image
945C: MOV ADCBUF2, W1      ; high image
```

NoHWMod low/high ADC ISR behavior:

```asm
385C: RCALL 0x5B00
945C: RCALL 0xA220
```

The new helper is:

```asm
MOV ADCBUF2, W0
MOV #0x6AC2, W2
MUL.UU W0, W2, W0
SL W1, #1, W1
LSR W0, #15, W0
IOR W0, W1, W1
RETURN
```

That is equivalent to:

```c
uint16_t scaled_vout = ((uint32_t)ADCBUF2 * 0x6AC2u) >> 15;
```

Numerically:

- `0x6AC2 / 32768 = 0.834045410156250`
- physical-voltage multiplier for unchanged thresholds: `32768 / 0x6AC2 = 1.198975484815`
- approximate increase: `19.898%`

Because the downstream code still writes the result to `0x0A00` and low-pass filters it into `0x0A02`, every regulation/protection path that consumes those variables now sees a lower apparent output voltage. This is the cleanest explanation for a no-hardware-mod output-voltage increase and a corresponding OVP shift.

## Raised float constant

The second patch changes one word in the low image and one mirrored word in the high image:

```asm
stock:    39B2 MOV #0xEF9E,W2
stock:    39B4 MOV #0x46C3,W3      ; literal 0x46C3EF9E = 25079.808594
modified: 39B2 MOV #0xEF9E,W2
modified: 39B4 MOV #0x46E4,W3      ; literal 0x46E4EF9E = 29303.808594
```

The same mirror appears at `0x95B4` in the high application image. This block feeds compiler float helpers and then stores an integer command to `0x09EC`, which is later ramped into the PWM4/PDC4 output-control path. The literal increased by `4224.000000` counts, or about `16.842%`.

## Interpretation

- The **ADC scaling hook** is the primary output-voltage/OVP behavior change. It causes the firmware to believe output voltage is lower than it really is, so the unchanged control and OVP thresholds occur at a higher physical voltage.
- The **float literal change** appears to be a companion threshold/setpoint adjustment in the PWM4/output-command calculation path.
- The bootloader/reset selector and application main loop structure are otherwise unchanged.
- The modified file/register and SFR dumps differ only in debugger/snapshot state, not meaningful firmware configuration.
