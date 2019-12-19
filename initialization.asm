; The following is a guard clause, to avoid this file from being included twice.
if !def(__initialization_asm__)
__initialization_asm__ set 1


include "hardware.inc"
; include "utility.asm"
section         "__initialization_asm__", ROM0


; `void TurnOffTheScreen()`
;
; Shut down all LCD-related capabilities.
; - Destroys: `af`
; - Changes: `rLY`, `rLCDC`
TurnOffTheScreen:
                push    hl
.waitVBlank:    ld      a, [rLY]
                cp      144                 ; Check if the LCD is past VBlank
                jr      c, .waitVBlank
;       We only need to reset a value with bit 7 reset
                ld      hl, rLCDC
                res     7, [hl]
                pop     hl
                ret


; `void EnsureCPUDoubledSpeed()`
;
; Ensures that the Game Boy Color is at maximum speed.
; - Destroys: `af`
EnsureCPUDoubledSpeed:
                ld      a, [rKEY1]
                bit     7, a
                jr      nc, .DoubleCGBSpeed
;       To change the speed, it's just a matter of (re)setting the bit 0 at FF4D
;       (also known as KEY1).
.SingleGBSpeed: or      $01
                stop
.DoubleCGBSpeed ret



; End of the guard clause.
endc ; __initialization_asm__


