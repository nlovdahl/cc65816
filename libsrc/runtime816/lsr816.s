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

        .export toslsr816_byte, toslsr816_word, toslsr816_long
        ; Note: tmp2 is implicitly used.
        .importzp sreg, tmp1

.proc   toslsr816_byte

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
        lsr
        dex
        bne @BitShift

@Done:
        rep #$20

        rts

.endproc

.proc   toslsr816_word

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
        and #$FF00
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
        lsr
        dex
        bne @BitShift

@Done:
        rts

.endproc

.proc   toslsr816_long

        cmp #$0020
        bcc @HiWordCheck
        ; if we get here, this would just shift everything away
        stz sreg
        lda #$0000
        rts

@HiWordCheck:
        tax
        lda $03,s
        bne @WordShiftCheck
        ; if we get here, then we can treat this like a word shift
        stz sreg
        txa
        jmp toslsr816_word

@WordShiftCheck:
        ; if we get here, we will need to work with sreg when shifting (for now)
        stx tmp1
        tax
        lda #$0010
        trb tmp1
        beq @FullByteShiftCheck
        ; if we get here, we can take a shortcut by working with the high word
        stz sreg

        ; this portion of the shifting routine only works with the low word...
@HalfByteShiftCheck:
        lda #$0008
        trb tmp1
        beq @HalfBitShiftPrep
        ; if we get here, we can take a shortcut using xba
        txa
        and #$FF00
        xba
        ldx tmp1
        bne @HalfBitShift
        ; if we get here, we don't need to shift any more
        rts

@HalfBitShiftPrep:
        txa
        ldx tmp1
        beq @Done

@HalfBitShift:
        lsr
        dex
        bne @HalfBitShift

        rts

        ; this portion of the shifting routine works with sreg too...
@FullByteShiftCheck:
        lda #$0008
        trb tmp1
        beq @FullBitShiftPrep
        ; if we get here, we can take a shortcut using xba and loading A
        txa
        xba
        tax
        and #$00FF
        sta sreg
        ; see if we can skip the high word now
        beq @FullToHalfPrep
        ; otherwise, we just proceed to keep working with both sreg and A
        txa
        sep #$20
        lda $02,s
        rep #$20
        ldx tmp1
        bne @FullBitShift
        ; if we get here, we don't need to shift any more
        rts
@FullToHalfPrep:
        ; if we get here, we would have been shifting a byte - finish that
        txa
        sep #$20
        lda $02,s
        rep #$20
        ldx tmp1
        bra @HalfBitShift

@FullBitShiftPrep:
        stx sreg
        lda $01,s
        ldx tmp1
        beq @Done

@FullBitShift:
        lsr sreg
        ror
        dex
        bne @FullBitShift

@Done:
        rts

.endproc

.endif
