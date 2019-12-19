; The following is a guard clause, to avoid this file from being included twice.
if !def(__graphics_asm__)
__graphics_asm__ set 1

include "hardware.inc"


section         "__graphics_asm__", ROM0
LittleRock:     
                db      $FF, $00, $7E, $FF, $85, $81, $89, $83 
                db      $93, $85, $A5, $8B, $C9, $97, $7E, $FF
LittleRock_END:


section         "__graphics_asm__", ROM0
; `void LoadLittleRockIntoBGTilemap()`
;
; Copy a Sprite into BG as Tile $80.
; - Destroys: `af`
LoadLittleRockIntoBGTilemap:
; CopyShortDataFromInto destroys bc, de, and hl, so we should save them
                push    bc
                push    de
                push    hl
                
                ld      de, LittleRock
                ld      hl, $8000
                ld      b, LittleRock_END - LittleRock

                call    CopyShortDataFromInto

                pop     hl
                pop     de
                pop     bc

                ret



; `void ChangePaletteByte(u16_bc RGB5, u8_d pal_index)`
;
; Configure a color in the Pallete Array to the color in BC.
;
; pal_index bytes 0-7 is pal0, 8-15 is pal1, 16-23 is pal2, and so on
;
; - Destroys: `d`, `hl`
ChangePaletteByte:
                ld      hl, rBCPS

                ld      [hl], d     ; D is a index into the palette array
                inc     hl          ; Points to rBCPD
                ld      [hl], c     ; Writes the lowest byte of the color

                inc     d           ; D should now point to the next byte

                dec     hl          ; Points to rBCPS
                ld      [hl], d
                inc     hl          ; Points to rBCPD
                ld      [hl], b     ; Writes the highest byte of the color

                ret


; End of the guard clause.
endc ; __graphics_asm__


