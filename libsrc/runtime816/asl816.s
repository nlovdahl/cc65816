;~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
; Copyright (C) 2025, Nicholas Lovdahl
;
; This software is provided 'as-is', without any expressed or implied warranty.
; In no event will the authors be held liable for any damages arising from the
; use of this software.
;
; Permission is granted to anyone to use this software for any purpose,
; including commercial applications, and to alter it and redistribute it freely,
; subject to the following restrictions:
;
; 1. The origin of this software must not be misrepresented; you must not claim
;    that you wrote the original software. If you use this software in a
;    product, an acknowledgment in the product documentation would be
;    appreciated but is not required.
; 2. Altered source versions must be plainly marked as such, and must not be
;    misrepresented as being the original software.
; 3. This notice may not be removed or altered from any source distribution.
;~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

        .macpack cpu

.if (.cpu .bitand ::CPU_65816)

.smart on
.a16
.i16

        .export tosasl816_byte, tosasl816_word, tosasl816_long
        ; Note: tmp2 is implicitly used.
        .importzp sreg, tmp1

.proc tosasl816_byte

        and #$00FF ; this is a surprise tool that will help us later (tax)
        sep #$20

        bne @BitShiftCheck
        ; if we get here, this is essentially a load (no shifting to do)
        lda $01,s
        bra @Done

@BitShiftCheck:
        cmp #$08
        bcc @BitShiftPrep
        ; if we get here, this would just shift everything away
        lda #$00
        bra @Done

@BitShiftPrep:
        tax ; will transfer all 16-bits from A since indexes are 16-bit
        lda $01,s

@BitShift:
        asl
        dex
        bne @BitShift

@Done:
        rep #$20

        rts

.endproc

.proc tosasl816_word

        cmp #$0010
        bcc @ByteShiftCheck
        ; if we get here, this would just shift everything away
        lda #$0000
        rts

@ByteShiftCheck:
        sta tmp1
        lda #$0008
        trb tmp1
        beq @BitShiftPrep
        ; if we get here, we can take a shortcut using xba
        lda $01,s
        and #$00FF
        xba
        ldx tmp1
        bne @BitShift
        ; if we get here, we don't need to shift any more
        rts

@BitShiftPrep:
        lda $01,s
        ldx tmp1
        beq @Done

@BitShift:
        asl
        dex
        bne @BitShift

@Done:
        rts

.endproc

.proc tosasl816_long

        cmp #$0020
        bcc @WordShiftCheck
        ; if we get here, this would just shift everything away
        stz sreg
        lda #$0000
        rts

@WordShiftCheck:
        ; we'll only need the low word on the stack if we're shifting past 16
        sta tmp1
        lda #$0010
        trb tmp1
        beq @FullByteShiftCheck
        ; if we get here, we only need to do the low word and store it in sreg
        lda tmp1
        jsr tosasl816_word
        sta sreg
        lda #$0000

        rts

        ; this portion of the shifting routine needs to handle both words
@FullByteShiftCheck:
        lda #$0008
        trb tmp1
        beq @FullBitShiftPrep
        ; if we get here, we can take a shortcut using xba
        lda $02,s
        sta sreg
        lda $01,s
        and #$00FF
        xba
        ldx tmp1
        bne @FullBitShift
        ; if we get here, we don't need to shift any more
        rts

@FullBitShiftPrep:
        ldx tmp1
        beq @Done
        lda $03,s
        sta sreg
        lda $01,s

@FullBitShift:
        asl
        rol sreg
        dex
        bne @FullBitShift

@Done:
        rts

.endproc

.endif
