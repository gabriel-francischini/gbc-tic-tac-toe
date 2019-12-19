; The following is a guard clause, to avoid this file from being included twice.
if !def(__utility_asm__)
__utility_asm__ set 1



section         "__utility_asm__", ROM0
; `void CopyDataFromInto()`
;
; Copy a Sprite into BG as Tile $80.
;
; Params:
; - `b`: number of bytes to copy
; - `de`: start (smallest) address of SOURCE
; - `hl`: start (smallest) address of DESTINATION
;
; Destroys: `af`, `b`, `de`, `hl`
CopyShortDataFromInto:
;       .CopyOneMore is a 
;           do {
;               *hl++ = *de++; // Copy a byte
;               b--;
;           } while(b > 0);
.CopyOneMore:   ld      a, [de]         ; Copies 1 byte
                ld      [hli], a        ; Points to next byte
                inc     de
                dec     b                      
;       Checks if we copied enough bytes (b == 0)
.WhileCheck:    xor     a
                or      b
                jr      nz, .CopyOneMore
.Done:          ret



; `void RepeatByteIntoArray()`
;
; Copy a Sprite into BG as Tile $80.
;
; Params:
; - `b`: number of times to copy
; - `d`: byte value to write
; - `hl`: start (smallest) address of DESTINATION
;
; Destroys: `af`, `b`,  `hl`
RepeatByteIntoArray:
;       .CopyOneMore is a 
;           do {
;               *hl++ = d; // Copy a byte
;               b--;
;           } while(b > 0);
.CopyOneMore:   ld      a, d            ; Copies 1 byte
                ld      [hli], a        ; Points to next byte
                dec     b
;       Checks if we copied enough bytes (b == 0)
.WhileCheck:    xor     a
                or      b
                jr      nz, .CopyOneMore
.Done:          ret






; End of the guard clause.
endc ; __utility_asm__


