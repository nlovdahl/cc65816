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

        .export tosasr816_byte, tosasr816_word, tosasr816_long
        .import toslsr816_byte, toslsr816_word, toslsr816_long
        .importzp sreg, tmp1

.proc tosasr816_byte

        ; see if we can just delegate to lsr, or if we need to mask with 1s
        and #$00FF ; this is a surprise tool that will help us later (tax & lda)
        tax
        lda $01,s
        bit #$0080
        bne @NegativeValue ; leading 1, so we should fill 1s as we shift
        ; otherwise, we can just jump to lsr
        txa
        jmp toslsr816_byte

@NegativeValue:
        cpx #$0000
        bne @BitShiftCheck
        ; if we get here, this is essentially a load (no shifting to do)
        rts

@BitShiftCheck:
        cpx #$0008
        bcc @BitShiftPrep
        ; if we get here, this would just shift everything away
        lda #$00FF
        rts

@BitShiftPrep:
        ora #$FF00 ; fill in 1s to shift into the first byte

@BitShift:
        lsr
        dex
        bne @BitShift

        rts

.endproc

.proc tosasr816_word

        ; see if we can just delegate to lsr, or if we need to mask with 1s
        tax
        lda $01,s
        bmi @NegativeValue ; leading 1, so we should fill 1s as we shift
        ; otherwise, we can just jump to lsr
        txa
        jmp toslsr816_word

NegativeWordValue:
@NegativeValue:
        cpx #$0010
        bcc @ByteShiftCheck
        ; if we get here, this would just shift everything away
        lda #$FFFF
        rts

@ByteShiftCheck:
        stx tmp1
        tax
        lda #$0008
        trb tmp1
        beq @BitShiftPrep
        ; if we get here, we can take a shortcut using xba
        txa
        ora #$00FF
        xba
        ldx tmp1
        bne @BitShift
        ; if we get here, we don't need to shift any more
        rts

@BitShiftPrep:
        txa
        ldx tmp1
        beq @Done

@BitShift:
        sec
        ror
        dex
        bne @BitShift

@Done:
        rts

.endproc

.proc tosasr816_long

        ; see if we can just delegate to lsr, or if we need to mask with 1s
        tax
        lda $03,s
        bmi @NegativeValue ; leading 1, so we should fill 1s as we shift
        ; otherwise, we can just jump to lsr
        txa
        jmp toslsr816_long

@NegativeValue:
        cpx #$0020
        bcc @HiWordCheck
        ; if we get here, this would just shift everything away
        lda #$FFFF
        sta sreg
        rts

@HiWordCheck:
        cmp #$FFFF
        bne @WordShiftCheck
        ; if we get here, then we can treat this like a word shift
        sta sreg
        jmp NegativeWordValue

@WordShiftCheck:
        ; if we get here, we will need to work with sreg when shifting (for now)
        stx tmp1
        tax
        lda #$0010
        trb tmp1
        beq @FullByteShiftCheck
        ; if we get here, we can take a shortcut by working with the high word
        lda #$FFFF
        sta sreg

        ; this portion of the shifting routine only works with the low word...
@HalfByteShiftCheck:
        lda #$0008
        trb tmp1
        beq @HalfBitShiftPrep
        ; if we get here, we can take a shortcut using xba
        txa
        ora #$00FF
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
        sec
        ror
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
        ora #$FF00
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
        sec
        ror sreg
        ror
        dex
        bne @FullBitShift

@Done:
        rts

.endproc

.endif
