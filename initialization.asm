; The following is a guard clause, to avoid this file from being included twice.
if !def(__initialization_asm__)
__initialization_asm__ set 1


include "hardware_registers.asm"
section         "game_code", ROM0


; `void TurnOffTheScreen()`
;
; Shut down all LCD-related capabilities.
; - Destroys: `af`
; - Changes: `rLY`, `rLCDC`
TurnOffTheScreen:
.waitVBlank:    ld      a, [rLY]
                cp      144                 ; Check if the LCD is past VBlank
                jr      c, .waitVBlank
;       We only need to reset a value with bit 7 reset, but 0 does the job.
                xor     a
                ld      [rLCDC], a          ; We will have to write to LCDC 
                                            ; again later, so it's not a bother,
                                            ; really.
                ret

; End of the guard clause.
endc ; __initialization_asm__