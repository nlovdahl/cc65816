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

        .export tosdiv816_ubyte, tosdiv816_uword, tosdiv816_ulong
        .export tosdiv816_sbyte, tosdiv816_sword, tosdiv816_slong
        ; Note: Though it isn't used, we import tmp4 to make sure it exists.
        .importzp sreg, tmp1, tmp2, tmp3, tmp4, ptr2, ptr3, ptr4

        ; Also note: division by 0 will result in undefined behavior.

.proc   tosdiv816_ubyte
; divide TOS by A (8-bit) -> 8-bit quotient in A, remainder in X
        sep #$20 ; setting index regs to 8 bits clears high byte, so we don't

        ; the quotient will be built using tmp1
        sta tmp2 ; the divisor will go in tmp2
        pla ; pull the numerator into A
        ; put the numerator's MSB in carry and add a dummy bit at the end
        asl
        ora #$01
        sta tmp3 ; the feed (numerator) will go in tmp3
        lda #$00 ; zero out the remainder but add the MSB from the numerator
        adc #$00
        bra @DivisorStep

@FeedStep:
        ; pull the next bit from the feed for the remainder
        asl tmp3
        beq @LoadResultAndReturn ; we're done once we consume the dummy bit
        rol
@DivisorStep:
        ; check the current remainder against the divisor
        cmp tmp2
        bcc @QuotientStep
        sbc tmp2 ; observe that carry must already be set for us to get here
        ; borrow should not be required, so carry should remain set
@QuotientStep:
        rol tmp1 ; add either a set or cleared bit to the quotient
        bra @FeedStep

@LoadResultAndReturn:
        ; we want the remainder in X and the quotient in A
        tax
        lda tmp1

        rep #$20

        rts

.endproc

.proc   tosdiv816_uword
; divide TOS by A (16-bit) -> 16-bit quotient in A, remainder in X
        ; the quotient will be built using tmp1 & tmp2
        sta tmp3 ; store the divisor in tmp3 & tmp4
        pla ; pull the numerator into A
        ; put the numerator's MSB in carry and add a dummy bit at the end
        asl
        ora #$0001
        sta ptr3 ; the feed (numerator) will go in ptr3
        lda #$0000 ; zero out the remainder but add the MSB from the numerator
        adc #$0000
        bra @DivisorStep

@FeedStep:
        ; pull the next bit from the feed for the remainder
        asl ptr3
        beq @LoadResultAndReturn ; we're done once we consume the dummy bit
        rol
@DivisorStep:
        ; check the current remainder against the divisor
        cmp tmp3
        bcc @QuotientStep
        sbc tmp3 ; observe that carry must already be set for us to get here
        ; borrow should not be required, so carry should remain set
@QuotientStep:
        rol tmp1 ; add either a set or cleared bit to the quotient
        bra @FeedStep

@LoadResultAndReturn:
        ; we want the remainder in X and the quotient in A
        tax
        lda tmp1

        rts

.endproc

.proc   tosdiv816_ulong
; divide TOS by A & sreg (32-bit) -> 32-bit quotient in A & sreg,
;                                    remainder in ptr3 and ptr4
        ; the quotient will be built using tmp1, tmp2, tmp3, and tmp4
        sta ptr3 ; store the divisor in ptr3 & ptr4
        lda sreg
        stz sreg
        sta ptr4
        bne @LowStepsShortcut
        pla ; pull the low word of the numerator
        ; put the numerator's MSB in carry and add a dummy bit at the end
        asl
        ora #$0001
        sta ptr2 ; the feed (numerator) will go in ptr2
        lda #$0000 ; zero out the remainder but add the MSB from the numerator
        adc #$0000
        bra @LowWordDivisorStep

        ; if we get to this part, we can assume the divisor is 1 word
@LowWordFeedStep:
        ; pull the next bit from the feed for the remainder
        asl ptr2
        beq @HighWordInit ; we're done once we consume the dummy bit
        rol
@LowWordDivisorStep:
        ; check the current remainder against the divisor
        cmp ptr3
        bcc @LowWordQuotientStep
        sbc ptr3 ; observe that carry must already be set for us to get here
        ; borrow should not be required, so carry should remain set
@LowWordQuotientStep:
        rol tmp1 ; add either a set or cleared bit to the quotient
        bra @LowWordFeedStep

@LowStepsShortcut:
        ; shortcuts we can take if the divisor is more than 1 word
        stz tmp1 ; quotient for the low part of the feed would be zero
        pla ; pull the low word of the numerator for the remainder

@HighWordInit:
        tax ; save the remainder so we can use A
        pla ; pull the high word of the numerator
        ; put the numerator's MSB in carry and add a dummy bit at the end
        asl
        ora #$0001
        sta ptr2 ; the feed (numerator) will go in ptr2
        ; roll the remainder to pick up the bit in carry
        txa
        rol
        rol sreg
        bra @HighWordDivisorStep

@HighWordFeedStep:
        ; pull the next bit from the feed for the remainder
        asl ptr2
        beq @LoadResultAndReturn ; we're done once we consume the dummy bit
        rol
        rol sreg
@HighWordDivisorStep:
        ; check the current remainder against the divisor
        ldx sreg
        cpx ptr4
        bcc @HighWordQuotientStep
        bne @HighWordSubtractionStep
        cmp ptr3
        bcc @HighWordQuotientStep
@HighWordSubtractionStep:
        sbc ptr3 ; observe that carry must already be set for us to get here
        sbc ptr4
        ; further borrow should not be required, so carry should remain set
@HighWordQuotientStep:
        rol tmp1 ; add either a set or cleared bit to the quotient
        rol tmp3
        bra @HighWordFeedStep

@LoadResultAndReturn:
        sta ptr3
        lda sreg
        sta ptr4
        lda tmp3
        sta sreg
        lda tmp1

        rts

.endproc

.proc   tosdiv816_sbyte
; divide TOS by A (8-bit) -> 8-bit quotient in A, remainder in X
        and #$00FF ; use the high byte to see if we need to negate the result
        sep #$20 ; setting index regs to 8 bits clears high byte, so we don't

        ; check and potentially adjust the numerator
        bit #$80
        bpl @CheckDenominator
        ; if we get here, we'll negate the numerator
        xba
        inc
        xba
        eor #$FF
        inc

        ; check and potentially adjust the denominator
@CheckDenominator:
        tax
        lda $01,s
        bpl @DoneCheckingNumAndDom
        ; if we get here, we'll negate the denominator
        xba
        inc
        xba
        eor #$FF
        inc
        sta $01,s

@DoneCheckingNumAndDom:
        ; see if we need to negate the result
        xba
        lsr
        bcs @CallDivAndNegate
        ; if we get here, we can just jump to the division
        txa
        rep #$20
        jmp tosdiv816_ubyte

@CallDivAndNegate:
        txa
        rep #$20 ; don't deal with sign extension
        jsr tosdiv816_ubyte
        sep #$20
        eor #$FF
        inc
        xba
        txa
        eor #$FF
        inc
        tax
        xba
        rep #$20

        rts

.endproc

.proc   tosdiv816_sword
; divide TOS by A (16-bit) -> 16-bit quotient in A, remainder in X
        stz tmp1 ; bit 0 (LSB) indicates if we need to negate the final result

        ; check and potentially adjust the numerator
        bit #$8000
        bpl @CheckDenominator
        ; if we get here, we'll negate the numerator
        inc tmp1
        eor #$FFFF
        inc

        ; check and potentially adjust the denominator
@CheckDenominator:
        tax
        lda $01,s
        bpl @DoneCheckingNumAndDom
        ; if we get here, we'll negate the denominator
        inc tmp1
        eor #$FFFF
        inc
        sta $01,s

@DoneCheckingNumAndDom:
        ; see if we need to negate the result
        lsr tmp1
        bcs @CallDivAndNegate
        ; if we get here, we can just jump to the division
        txa
        jmp tosdiv816_uword

@CallDivAndNegate:
        txa
        jsr tosdiv816_uword
        eor #$FFFF
        inc
        sta tmp1
        txa
        eor #$FFFF
        inc
        tax
        lda tmp1

        rts

.endproc

.proc   tosdiv816_slong
; divide TOS by A & sreg (32-bit) -> 32-bit quotient in A & sreg,
;                                    remainder in ptr3 and ptr4
        stz tmp1 ; bit 0 (LSB) indicates if we need to negate the final result
        tax ; save a copy of A in X

        ; check and potentially adjust the numerator
        lda sreg
        bpl @CheckDenominator
        ; if we get here, we'll negate the numerator
        inc tmp1
        eor #$FFFF
        sta sreg
        txa
        eor #$FFFF
        inc
        tax
        bne @CheckDenominator
        inc sreg

        ; check and potentially adjust the denominator
@CheckDenominator:
        lda $03,s
        bpl @DoneCheckingNumAndDom
        ; if we get here, we'll negate the denominator
        inc tmp1
        lda $01,s
        eor #$FFFF
        clc
        adc #$0001
        sta $01,s
        lda $03,s
        eor #$FFFF
        adc #$0000
        sta $03,s

@DoneCheckingNumAndDom:
        ; see if we need to negate the result
        lsr tmp1
        bcs @CallDivAndNegate
        ; if we get here, we can just jump to the division
        txa
        jmp tosdiv816_ulong

@CallDivAndNegate:
        txa
        jsr tosdiv816_ulong
        eor #$FFFF
        clc
        adc #$0001
        tax
        lda sreg
        eor #$FFFF
        adc #$0000
        sta sreg
        lda ptr3
        eor #$FFFF
        clc
        adc #$0001
        sta ptr3
        lda ptr4
        eor #$FFFF
        adc #$0000
        sta ptr4
        txa

        rts

.endproc

.endif
