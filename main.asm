include "hardware.inc"
include "initialization.asm"
include "utility.asm"
include "graphics/graphics.asm"


; This is literally the Header of the ROM and the entrypoint for the CPU into
; our code/ROM/catridge.
section         "header", ROM0[$100]
EntryPoint:     di                          ; Interrupts are messy right now
                jp      Main

;   Because this is the HEADER, we should reserve some bytes so the HEADER
;   could get fixed with the Nintendo's logo. Otherwise the resulting ROM
;   from this program would be considerated invalid/corrupted.
                rept    $143 - $104
                db      0
                endr

section         "header_cgb", ROM0[$143]
                db      $C0
                rept    $150 - $143
                db      0
                endr

section         "minimum_stack", WRAM0[$CD00]
StackMinTop:    ds $2FF
StackMaxBottom:

section         "game_code", ROM0
; `void main()`
;
; Where the code execution really begins.
; - Trashes: everything
; - Returns: never
Main:
;   There are a couple of things we need to do as we starts the ROM up:
;       1. Set up the global stack -- Already done for us by the Game Boy's
;          bootstrap ROM. But better set it to RAM instead of HRAM.
;          See: https://gbdev.gg8.se/wiki/articles/Gameboy_Bootstrap_ROM#Contents_of_the_ROM
;       2. Disable the LCD screen and the audio, so we can config everything
;          without external interference (PPU locking the VRAM, etc).

;   Clears the space for the stack and sets it
                ld      d, $00
                ld      bc, StackMaxBottom - StackMinTop + 1
                ld      hl, StackMinTop
                call    RepeatByteIntoLongArray

                ld      sp, $D000                 ; Effectively starts @ $CFFF

                call    EnsureCPUDoubledSpeed
                call    TurnOffTheScreen
                call    InitializeVRAM
                call    InitializeRAM
                call    InitializeTileAddrArray
                call    LoadLittleRockIntoBGTilemap

;    Puts the LittleRock on the BG Scren
;    Fills the 32*32 tilemap/screen in groups of 4 each
                ld      b, 18
                ld      d, $01
                ld      hl, _SCRN0 + 32 * 1 + 1
                call    RepeatByteIntoArray

                ld      b, 16
                ld      d, $01
                ld      hl, _SCRN0 + 32 * 4 + 2
                call    RepeatByteIntoArray

;    Puts the correct palette that LittleRock uses
                push    de
                ld      de, CG_Palette__artwork__0
                call    LoadBGPaletteBank
                pop     de

                xor     a
                ld      [rSCY], a
                ld      [rSCX], a

                ; Shut sound down
                ld      [rNR52], a

                ; Turn screen on
                push    hl
                ld      hl, rLCDC
                set     4, [hl]     ; BG & Window Tile Data Select = 8000-8FFF %(0)
                res     3, [hl]     ; BG Tile Map Display Select = 9800-9BFF (%0)
                res     2, [hl]     ; OBJ (Sprite) Size = 8x8 (%0)
                set     1, [hl]     ; OBJ (Sprite) Display Enable = On (%1)
                set     7, [hl]     ; LCD Display Enable = On (%1)
                pop     hl

.LockUp:        jp      .LockUp



section         "game_data", ROM0
;                   xBbBbBGgGgGRrRrR
MainPalette:    dw %0111110000000000
                dw %0111111111100000
                dw %0000000000011111
                dw %0000001111111111

include "graphics/artwork/compiled_artwork.asm"