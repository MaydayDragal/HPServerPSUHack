; PL30 Rev.06 secondary 20A CC / ~14.4 V patch reference
; Target: dsPIC33FJ64GS606

; VOUT threshold
; 0x3348 / 0x8F48: stock 203700 MOV #0x370,W0 -> modified 203B20 MOV #0x3B2,W0

; CC/current profile constant
; 0x3446 / 0x9046: stock 255110 MOV #0x5511,W0 -> modified 210000 MOV #0x1000,W0

; Output command float constant
; 0x39B2 / 0x95B2 unchanged: MOV #0xEF9E,W2
; 0x39B4 / 0x95B4: stock MOV #0x46C3,W3 -> modified MOV #0x46E8,W3
; stock bits 0x46C3EF9E = 25079.8086f
; modified bits 0x46E8EF9E = 29815.8086f
