; The following is a guard clause, to avoid this file from being included twice.
if !def(__initialization_asm__)
__initialization_asm__ set 1


include "hardware.inc"
include "utility.asm"
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


; `void InitializeVRAM()`
;
; Shut down all LCD-related capabilities.
; - Destroys: `af`,
; - Changes: `rLY`, `rLCDC`
InitializeVRAM:
                push    bc
                push    de
                push    hl

;       Loads the VRAM Bank 0 (bit 0 of rVBK == 0)
                ld      hl, rVBK
                res     0, [hl]

;       Clear the 3 blocks of VRAM Tile Data with 0, each block with 128 tiles
;       Block 0 ($8000-87FF) is for sprites and normally BG/Window
;       Block 1 ($8800-8FFF) is for sprites and BG/Window
;       Block 2 ($9000-97FF) can only be used by BG/Window (under 8800
;               signed addressing).
                ld      d, $00
                ld      bc, ($97FF + 1) - _VRAM
                ld      hl, _VRAM
                call    RepeatByteIntoLongArray

;       Clear the 2 BG Tile Maps, each one with 32x32 tiles
;       (9800h-9BFFh and 9C00h-9FFFh)
                ld      d, $ff                    ; Points to a unused tile
                ld      bc, ($9FFF + 1) - _SCRN0
                ld      hl, _SCRN0
                call    RepeatByteIntoLongArray

;       Loads the VRAM Bank 1 (bit 0 of rVBK == 1)
                ld      hl, rVBK
                set     0, [hl]

;       Clear the 3 blocks of VRAM Tiles with 0, like the ones on VRAM bank 0
                ld      d, $00
                ld      bc, ($97FF + 1) - _VRAM
                ld      hl, _VRAM
                call    RepeatByteIntoLongArray

;       Clear the 2 BG Tile ATTRIBUTES
                ld      d, $00
                ld      bc, ($9FFF + 1) - _SCRN0
                ld      hl, _SCRN0
                call    RepeatByteIntoLongArray

;       Unloads the VRAM Bank 1/ Loads VRAM Bank 0
                ld      hl, rVBK
                res     0, [hl]

;       Clears the VRAM Sprite ATTRIBUTE Table (OAM).
;       The Sprite Patterns (tiles) is in $8000-$8FFF and have
;       the same format of BG tiles.
                ld      d, $00
                ld      bc, ($FE9F + 1) - _OAMRAM
                ld      hl, _OAMRAM
                call    RepeatByteIntoLongArray


;       Clears the Background Color Palettes
                ld      a, %10000000    ; sets BCPS to increment after writing
                ld      [rBCPS], a      ; and points it to palette index 0

;       We have 8 palettes, each one spanning 8 bytes
;       4 colors * 2 byte-per-color = 8 bytes per palette
                ld      b, (8 * 4 * 2)
                ld      hl, rBCPD

;       .loop is:
;           while (b != 0){
;               *hl = 0;           // HL points to an I/O port so
;               b--;               // we should not increment it, BCPS does that
;           }
.loopBG:        xor     a
                or      b
                jr      z, .doneBG

                ld      a, $00
                ld      [hl], a
                dec     b
                jr      .loopBG
.doneBG:

;       Clears the Sprites/Objects Color Palettes
;       Same code as above, it's just different registers
                ld      a, %10000000
                ld      [rOCPS], a

                ld      b, (8 * 4 * 2)
                ld      hl, rOCPD


.loopOBJ:       xor     a
                or      b
                jr      z, .doneOBJ

                ld      a, $00
                ld      [hl], a
                dec     b
                jr      .loopOBJ
.doneOBJ:



;       Clear the BG scrolling
                xor     a
                ld      [rSCY], a
                ld      [rSCX], a

;       Clear the Window scrolling
                inc     a
                ld      [rWY], a
                add     7
                ld      [rWX], a

;       Clear the LCDC Y-Coordinate Comparator
                ld      a, $FF
                ld      [rLYC], a


                pop     hl
                pop     de
                pop     bc

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


