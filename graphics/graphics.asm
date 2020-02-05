; The following is a guard clause, to avoid this file from being included twice.
if !def(__graphics_asm__)
__graphics_asm__ set 1

include "hardware.inc"
include "utility.asm"


section         "__graphics_asm__", ROM0
LittleRock:
                db      $FF, $00, $7E, $FF, $85, $81, $89, $83
                db      $93, $85, $A5, $8B, $C9, $97, $7E, $FF
LittleRock_END:


section         "scratch_vram_in_ram", WRAMX[$D000], BANK[2]
; Each bytes here represents how many times each of tiles in BANKs 0-1 is
; currently being used.
TileUsageArray: ds 256


section         "scratch_vram_in_ram__addrtiles", WRAMX[$D100], BANK[2]
                rsreset
AddrTile_addr   rw      1
AddrTile_tile   rb      1
AddrTile_SIZEOF rb      0

; Every 3 bytes heres maps a address of which tile is currently loaded
; in a tile index (of BANKs 0-1).
TileAddrArray:  ds      AddrTile_SIZEOF * 256
.size:          ds      1   ; The real size of the array. Should be 256 (or 255)
.length:        ds      1   ; The *virtual* size/length of the array
.level:         ds      1   ; How many times we "expanded" the virtual array
.spacing:       ds      1   ; How far apart consecutive elements in the virtual
                            ;   array are in the real array. It's the log2 of
                            ;   that number, to be more exact.
.SIZEOF:


section         "scratch_loop_vars", HRAM
GuessIndex:     ds      1


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
ChangeBGPaletteByte:
                ld      hl, rBCPS


                ld      a, d        ; Set the auto-increment bit to rBCPS
                or      %10000000

                ld      [hl], d     ; D is a index into the palette array
                inc     hl          ; Points to rBCPD
                ld      [hl], c     ; Writes the lowest byte of the color
                                    ; rBCPS should auto increment to next byte
                ld      [hl], b     ; Writes the highest byte of the color

                ret

; `void LoadBGPaletteBank(u16_de PaletteBankROMAddress)`
;
; Configures the BG Palette bank to match the colors in the address BC.
;
; - Destroys: `b`, `de`, `hl`
LoadBGPaletteBank:
                xor     a
                or      %10000000        ; Set the auto-increment bit of rBCPS
                ld      [rBCPS], a
                ld      hl, rBCPD        ; Points the Palette Index back to 0
                ld      b, 8 * 8         ; We have 8 palettes of 8 bytes each

;       .loop is a:
;           do {
;               *hl++ = *de++; // Copy a byte
;               b--;
;           } while(b > 0);
.loop:          ld      a, [de]
                ld      [hl], a         ; rBCPD autoincrements

                inc     de
                dec     b

                xor     a
                or      b
                jr      nz, .loop

                ret


; `void InitializeTileAddrArray()`
;
; Initializes the bookkeeping of the VRAM's tiles.
; UNSAFEly clears the array and sets it back to a valid state.
; - Destroys `bc`, `d`, `hl`
InitializeTileAddrArray:
                ld      bc,  TileAddrArray.SIZEOF - TileAddrArray
                ld      d, $00
                ld      hl, TileAddrArray
                call    RepeatByteIntoLongArray

                ld      a, 255
                ld      [TileAddrArray.size], a

                ld      a, 3
                ld      [TileAddrArray.length], a

                ld      a, 6        ; 6 = log(256) - log(4) = 8 - 2 = 6
                ld      [TileAddrArray.spacing], a

                ret


; `(u16_bc) AddrTile* TileAddr::BinarySearch(u16_bc Key)`
;
; Finds the address corresponding to Key in the TileAddrArray.
; - Destroys: `de`, `hl`
TileAddr_BinarySearch:
                ld      d, 0                        ; Lower bound index
                ld      a, [TileAddrArray.length]   ; Higher bound index
                ld      e, a


;       Calculates: u8 a = (d + e) // 2;
;       This is our guess to where we should look next for the entry
.ComputeGuess:  ; Label for debugger
                xor     a
                add     d
                add     e
                sra     a

;           We will use this guess to adjust our lower/higher bound afterwards
                ldh     [GuessIndex], a
                push    de

;       We'll use HL to calculate the effective address of the median index
;       between virtual indexes D and E (which is currently held in A)
                ld      h, TileAddrArray >> 8
                xor     l

;       Converts the virtual index at A to a real index by considering the
;       spacing between consecutive elements in the real array. So, for example,
;       if TileAddrArray.spacing is 6, it means that the VIRTUAL indexes 0 and 1
;       are actually 2**6 (= 64) indexes apart when mapped into the REAL array.
;       Finally, the REAL index has to be mapped into an effective address.
;
;           u8 a = a << d;
                ld      a, [TileAddrArray.spacing]
                ld      d, a
                ldh     a, [GuessIndex]
.doSpacing:     sla     a
                dec     d
                jr      nz, .doSpacing

;       This is:
;           u16 hl = hl + 3 * a;
                ld      d, a
                add     d
                jr      nc, .noCarryAt2X
                inc     h
.noCarryAt2X:   add     d
                jr      nc, .noCarryAt3X
                inc     h
.noCarryAt3X:   ld      l, a


; End of the guard clause.
endc ; __graphics_asm__


