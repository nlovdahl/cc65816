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

        .export tosmul816_ubyte, tosmul816_uword, tosmul816_ulong
        ; Note: Though it isn't used, we import tmp4 to make sure it exists.
        .importzp sreg, tmp1, tmp2, tmp3, tmp4, ptr3, ptr4

; Note: Signed and unsigned multiplication should be identical if we only take
;       the lower part (ex: the low byte from the word result of an 8x8 mul).

.proc   tosmul816_ubyte
; multiply A and TOS (8-bit) -> 8-bit product in A
        sep #$20

        stz tmp1 ; zero out our running sum
        sta tmp2 ; store our 'feed'
        pla ; pull the TOS value to use as the value to be potentially added

        ; the first step...
        lsr tmp2
        beq @FinalStep
        bcc @MiddleStep
        sta tmp1

@MiddleStep:
        asl
        beq @LoadResultAndReturn
        lsr tmp2
        beq @FinalStep
        bcc @MiddleStep
        tax
        clc
        adc tmp1
        sta tmp1
        txa
        bra @MiddleStep

@FinalStep:
        bcc @LoadResultAndReturn
        clc
        adc tmp1
        bra @Done

@LoadResultAndReturn:
        lda tmp1
@Done:
        rep #$20

        rts

.endproc

.proc   tosmul816_uword
; multiply A and TOS (16-bit) -> 16-bit product in A
        stz tmp1 ; zero out our running sum (this takes tmp2 too)
        sta tmp3 ; store our 'feed' (this takes tmp4 too)
        pla ; pull the TOS value to use as the value to be potentially added

        ; the first step...
        lsr tmp3
        beq @FinalStep
        bcc @MiddleStep
        sta tmp1

@MiddleStep:
        asl
        beq @LoadResultAndReturn
        lsr tmp3
        beq @FinalStep
        bcc @MiddleStep
        tax
        clc
        adc tmp1
        sta tmp1
        txa
        bra @MiddleStep

@FinalStep:
        bcc @LoadResultAndReturn
        clc
        adc tmp1
        bra @Done

@LoadResultAndReturn:
        lda tmp1

@Done:
        rts

.endproc

.proc   tosmul816_ulong
; multiply A & sreg and TOS (32-bit) -> 32-bit result in A & sreg
        stz tmp1 ; zero out our running sum (we'll need tmp1 through tmp4)
        stz tmp3
        sta ptr3 ; save a copy of the multiplicand's low word for later...
        plx ; pull the first part of our feed
        beq @HighWordFirstStep
        stx ptr4 ; use ptr4 as a buffer for our feed

        ; the first step with the low word feed...
        lsr ptr4
        beq @LowWordLastStep
        bcc @LowWordMiddleStep
        sta tmp1
        ldx sreg
        stx tmp3

@LowWordMiddleStep:
        asl
        rol sreg
        lsr ptr4
        beq @LowWordLastStep
        bcc @LowWordMiddleStep
        tax
        clc
        adc tmp1
        sta tmp1
        lda sreg
        adc tmp3
        sta tmp3
        txa
        bra @LowWordMiddleStep

@LowWordLastStep:
        bcc @HighWordFirstStep
        clc
        adc tmp1
        sta tmp1
        lda sreg
        adc tmp3
        sta tmp3

        ; steps with the high word can focus on just that part of the sum
@HighWordFirstStep:
        plx ; pull the last part of our feed
        beq @LoadResultAndReturn
        stx ptr4
        lda ptr3

        ; the first step with the high word feed...
        lsr ptr4
        beq @HighWordLastStep
        bcc @HighWordMiddleStep
        tax
        clc
        adc tmp3
        sta tmp3
        txa

@HighWordMiddleStep:
        asl
        lsr ptr4
        beq @HighWordLastStep
        bcc @HighWordMiddleStep
        tax
        clc
        adc tmp3
        sta tmp3
        txa
        bra @HighWordMiddleStep

@HighWordLastStep:
        bcc @LoadResultAndReturn
        clc
        adc tmp3
        sta tmp3

@LoadResultAndReturn:
        lda tmp1
        ldx tmp3
        stx sreg

        rts

.endproc

.endif
