include "hardware_registers.asm"
include "initialization.asm"


; This is literally the Header of the ROM and the entrypoint for the CPU into
; our code/ROM/catridge.
section         "header", ROM0[$100]
EntryPoint:     di                          ; Interrupts are messy right now
                jp      Main

;   Because this is the HEADER, we should reserve some bytes so the HEADER 
;   could get fixed with the Nintendo's logo. Otherwise the resulting ROM
;   from this program would be considerated invalid/corrupted.
                rept    $150 - $104
                db      0
                endr



section         "game_code", ROM0
; `void main()`
;
; Where the code execution really begins.
; - Trashes: everything
; - Returns: never
Main:
;   There are a couple of things we need to do as we starts the ROM up:
;       1. Set up the global stack -- Already done for us by the Game Boy's
;          bootstrap ROM. 
;          See: https://gbdev.gg8.se/wiki/articles/Gameboy_Bootstrap_ROM#Contents_of_the_ROM
;       2. Disable the LCD screen and the audio, so we can config everything 
;          without external interference (PPU locking the VRAM, etc).
                call TurnOffTheScreen
                jp Main



