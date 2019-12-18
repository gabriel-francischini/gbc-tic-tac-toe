; The following is a guard clause, to avoid this file from being included twice.
if !def(__hardware_registers_asm__)
__hardware_registers_asm__ set 1

section         "game_code", ROM0

; ==============================================================================
; SOME GAMEBOY REGISTERS
; ------------------------------------------------------------------------------
; -- LY ($FF44)
; -- LCDC Y-Coordinate (R)
; -- Values range from 0->153. 144->153 is the VBlank period.
rLY equ $FF44

; -- LCDC ($FF40)
; -- LCD Control (R/W)
rLCDC EQU $FF40
; ==============================================================================



; End of the guard clause.
endc ; __hardware_registers_asm__