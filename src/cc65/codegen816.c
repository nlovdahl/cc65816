/*****************************************************************************/
/*                                                                           */
/*                               codegen816.c                                */
/*                                                                           */
/*                           65816 code generator                            */
/*                                                                           */
/*                                                                           */
/*                                                                           */
/* (C) 2022-2023, Nicholas Lovdahl                                           */
/*                                                                           */
/*                                                                           */
/* This software is provided 'as-is', without any expressed or implied       */
/* warranty.  In no event will the authors be held liable for any damages    */
/* arising from the use of this software.                                    */
/*                                                                           */
/* Permission is granted to anyone to use this software for any purpose,     */
/* including commercial applications, and to alter it and redistribute it    */
/* freely, subject to the following restrictions:                            */
/*                                                                           */
/* 1. The origin of this software must not be misrepresented; you must not   */
/*    claim that you wrote the original software. If you use this software   */
/*    in a product, an acknowledgment in the product documentation would be  */
/*    appreciated but is not required.                                       */
/* 2. Altered source versions must be plainly marked as such, and must not   */
/*    be misrepresented as being the original software.                      */
/* 3. This notice may not be removed or altered from any source              */
/*    distribution.                                                          */
/*                                                                           */
/*****************************************************************************/



#include <inttypes.h>

/* cc65 */
#include "asmlabel.h"
#include "codegen.h"
#include "error.h"
#include "segments.h"
#include "codegen816.h"

#define M_BYTE(Value)   ((Value) & 0xFF)
#define M_WORD(Value)   ((Value) & 0xFFFF)
#define M_DWORD(Value)  ((Value) & 0xFFFFFFFF)

static int CheckStackRelative(int Offset)
/* Check if an offset from the stack allows for stack-relative addressing. */
{
    return Offset >= 0 && Offset <= 255;
}

/*****************************************************************************/
/*                   Specialized functions for 65816 code                    */
/*****************************************************************************/



void g_816_leave(unsigned ToDrop)
/* Adds code to handle exiting a function before returning. */
{
    AddCodeLine("tax"); /* Save the return value. */
    AddCodeLine("tsc");
    AddCodeLine("clc");
    AddCodeLine("adc #$%04", M_WORD(ToDrop));
    AddCodeLine("tcs");
    AddCodeLine("txa");
}

void g_816_swap_regvars(int StackOffs, int RegOffs, unsigned Bytes)
/* Adds code to swap a value in memory with another in the stack. */
{
    unsigned BytesDone = 0;
    unsigned OddByte = Bytes % 2;
    int EndOffs = RegOffs + Bytes - 2 + OddByte;
    int StackRel = StackOffs + Bytes - 1 + OddByte;
    int UseSP = !(Bytes <= 8 &&
                  CheckStackRelative(StackOffs + 1) &&
                  CheckStackRelative(StackOffs + Bytes));

    /* If we can't use stack-relative addressing or need to loop... */
    if (UseSP) {
        AddCodeLine("tsc");
        AddCodeLine("clc");
        AddCodeLine("adc #$%04X", M_WORD(StackOffs + 1));
        AddCodeLine("sta sp");
        AddCodeLine("ldy #$%04X", M_WORD(Bytes - 2 + OddByte));
    }

    /* Unroll swaps for up to 8 bytes, or handle an odd byte... */
    if (OddByte || Bytes <= 8) {
        while (BytesDone < Bytes) {
            if (OddByte) { AddCodeLine("sep #$20"); }

            if (UseSP) {
                AddCodeLine("lda (sp),y");
            } else {
                AddCodeLine("lda $%02X,s", M_BYTE(StackRel - BytesDone));
            }
            AddCodeLine("ldx regbank%+d", EndOffs - BytesDone);
            AddCodeLine("sta regbank%+d", EndOffs - BytesDone);
            AddCodeLine("txa");
            if (UseSP) {
                AddCodeLine("sta (sp),y");
            } else {
                AddCodeLine("sta $%02X,s", M_BYTE(StackRel - BytesDone));
            }
            AddCodeLine("dey");

            if (OddByte) {
                AddCodeLine("rep #$20");
                OddByte = 0;
                BytesDone += 1;

                /* If we need to loop, we're not going to unroll. */
                if (Bytes > 8) { break; }
            } else {
                AddCodeLine("dey");
                BytesDone += 2;
            }
        }

        /* Return iff we've unrolled. */
        if (Bytes <= 8) { return; }
    }

    /* If we get here, we should be looping over more than 8 bytes. */
    unsigned Label = GetLocalLabel();

    g_defcodelabel(Label);
    AddCodeLine("lda (sp),y");
    AddCodeLine("ldx regbank%+d,y", RegOffs + BytesDone);
    AddCodeLine("sta regbank%+d,y", RegOffs + BytesDone);
    AddCodeLine("txa");
    AddCodeLine("sta (sp),y");
    AddCodeLine("dey");
    AddCodeLine("dey");
    AddCodeLine("bpl %s", LocalLabelName(Label));
}

void g_816_save_regvars(int RegOffs, unsigned Bytes)
/* Adds code to save some value or values in memory to the stack. */
{
    /* Unroll pushes to the stack for up to 8 bytes. */
    if (Bytes <= 8) {
        unsigned BytesDone = 0;
        unsigned OddByte = Bytes % 2;
        int EndOffs = RegOffs + Bytes - 2 + OddByte;

        while (BytesDone < Bytes) {
            if (OddByte) { AddCodeLine("sep #$20"); }

            AddCodeLine("lda regbank%+d", EndOffs - BytesDone);
            AddCodeLine("pha");

            if (OddByte) {
                AddCodeLine("rep #$20");
                OddByte = 0;
                BytesDone += 1;
            } else {
                BytesDone += 2;
            }
        }

        return;
    }

    /* Otherwise, we should move bytes in bulk. */
    AddCodeLine("tsc");
    AddCodeLine("tay");
    AddCodeLine("clc");
    AddCodeLine("sbc #$%04X", M_WORD(Bytes));
    AddCodeLine("tcs");
    AddCodeLine("ldx #regbank%+d", RegOffs + Bytes - 1);
    AddCodeLine("lda #$%04X", M_WORD(Bytes - 1));
    AddCodeLine("mvp ^regbank, ^regbank");
}

void g_816_restore_regvars(int StackOffs, int RegOffs, unsigned Bytes)
/* Adds code to save some value or values in the stack to memory. */
{
    /* Unroll restorations from the stack for up to 8 bytes. */
    if (Bytes <= 8) {
        unsigned BytesDone = 0;
        unsigned OddByte = Bytes % 2;
        int UseSP = !(CheckStackRelative(StackOffs + 1) &&
                      CheckStackRelative(StackOffs + Bytes));

        if (UseSP) {
            AddCodeLine("tsc");
            AddCodeLine("sta sp");
        }

        while (BytesDone < Bytes) {
            if (OddByte) { AddCodeLine("sep #$20"); }

            /* Try to use stack-relative addressing if we can. */
            if (UseSP) {
                /* Just load y instead of incrementing / deccrementing it. */
                AddCodeLine("ldy #$%04X", M_WORD(StackOffs + 1 + BytesDone));
                AddCodeLine("lda (sp),y");
            } else {
                AddCodeLine("lda $%02X,s", M_BYTE(StackOffs + 1 + BytesDone));
            }
            AddCodeLine("sta regbank%+d", RegOffs + BytesDone);

            if (OddByte) {
                AddCodeLine("rep #$20");
                OddByte = 0;
                BytesDone += 1;
            } else {
                BytesDone += 2;
            }
        }

        return;
    }

    /* Otherwise, we should move bytes in bulk. */
    AddCodeLine("tsc");
    AddCodeLine("clc");
    AddCodeLine("adc #$%04X", M_WORD(StackOffs + 1));
    AddCodeLine("tax");
    AddCodeLine("ldy #regbank%+d", RegOffs);
    AddCodeLine("lda #$%04X", M_WORD(Bytes - 1));
    AddCodeLine("mvn ^regbank, ^regbank");
}

void g_816_getimmed(unsigned Flags, uintptr_t Val, const char* LabelBuf)
/* Adds code to load a constant into the primary register. */
{
    if (LabelBuf != NULL) {
        AddCodeLine("lda #%s", LabelBuf);

        return;
    }

    switch (Flags & CF_TYPEMASK) {
        case CF_CHAR:
            if (Flags & CF_FORCECHAR) {
                AddCodeLine("lda #$00%02X", M_BYTE(Val));

                break;
            }
        /* Fall through... */
        case CF_INT:
            AddCodeLine("lda #$%04X", M_WORD(Val));
            break;

        case CF_LONG:
            AddCodeLine("lda #$04X", M_WORD(Val >> 16));
            AddCodeLine("sta sreg");
            if (M_WORD(Val >> 16) != M_WORD(Val)) {
                AddCodeLine("lda #$04X", M_WORD(Val));
            }
            break;

        default:
            TypeError(Flags);
    }
}

void g_816_getstatic(unsigned Flags, const char* LabelBuf)
/* Adds code to load a value from a static memory cell. */
{
    switch (Flags & CF_TYPEMASK) {
        case CF_CHAR:
            if (Flags & (CF_FORCECHAR | CF_TEST)) {
                AddCodeLine("sep #$20");
                AddCodeLine("lda %s", LabelBuf);
                AddCodeLine("rep #$20");
            } else {
                AddCodeLine("lda %s", LabelBuf);
                AddCodeLine("and #$00FF");

                /* Handle sign extension if we need to. */
                if (!(Flags & CF_UNSIGNED)) {
                    unsigned EndSignExtension = GetLocalLabel();

                    AddCodeLine("bit #$0080");
                    AddCodeLine("beq %s", LocalLabelName(EndSignExtension));
                    AddCodeLine("ora #$FF00");
                    g_defcodelabel(EndSignExtension);
                }
            }
            break;

        case CF_INT:
            AddCodeLine("lda %s", LabelBuf);
            break;

        case CF_LONG:
            AddCodeLine("lda %s+2", LabelBuf);

            if (Flags & CF_TEST) {
                AddCodeLine("ora %s", LabelBuf);
            } else {
                AddCodeLine("sta sreg");
                AddCodeLine("lda %s", LabelBuf);
            }
            break;

        default:
            TypeError(Flags);
    }
}

void g_816_getlocal(unsigned Flags, int StackOffs)
/* Adds code to load a value from the stack. */
{
    switch (Flags & CF_TYPEMASK) {
        case CF_CHAR:
            if (Flags & (CF_FORCECHAR | CF_TEST)) {
                AddCodeLine("sep #$20");
                AddCodeLine("lda $%02X,s", M_BYTE(StackOffs + 1));
                AddCodeLine("rep #$20");
            } else {
                AddCodeLine("lda $%02X,s", M_BYTE(StackOffs + 1));
                AddCodeLine("and #$00FF");

                /* Handle sign extension if we need to. */
                if (!(Flags & CF_UNSIGNED)) {
                    unsigned EndSignExtension = GetLocalLabel();

                    AddCodeLine("bit #$0080");
                    AddCodeLine("beq %s", LocalLabelName(EndSignExtension));
                    AddCodeLine("ora #$FF00");
                    g_defcodelabel(EndSignExtension);
                }
            }
            break;

        case CF_INT:
            AddCodeLine("lda $%02X,s", M_BYTE(StackOffs + 1));
            break;

        case CF_LONG:
            AddCodeLine("lda $%02X,s", M_BYTE(StackOffs + 3));

            if (Flags & CF_TEST) {
                AddCodeLine("ora $%02X,s", M_BYTE(StackOffs + 1));
            } else {
                AddCodeLine("sta sreg");
                AddCodeLine("lda $%02X,s", M_BYTE(StackOffs + 1));
            }
            break;

        default:
            TypeError(Flags);
    }
}

void g_816_getind(unsigned Flags, unsigned Offs)
/* Adds code to load a value indirectly. */
{
    AddCodeLine("sta ptr1");

    switch (Flags & CF_TYPEMASK) {
        case CF_CHAR:
            AddCodeLine("ldy #$%04X", M_WORD(Offs));

            if (Flags & (CF_FORCECHAR | CF_TEST)) {
                AddCodeLine("sep #$20");
                AddCodeLine("lda (ptr1),y");
                AddCodeLine("rep #$20");
            } else {
                AddCodeLine("lda (ptr1),y");
                AddCodeLine("and #$00FF");

                /* Handle sign extension if we need to. */
                if (!(Flags & CF_UNSIGNED)) {
                    unsigned EndSignExtension = GetLocalLabel();

                    AddCodeLine("bit #$0080");
                    AddCodeLine("beq %s", LocalLabelName(EndSignExtension));
                    AddCodeLine("ora #$FF00");
                    g_defcodelabel(EndSignExtension);
                }
            }
            break;

        case CF_INT:
            AddCodeLine("ldy #$%04X", M_WORD(Offs));
            AddCodeLine("lda (ptr1),y");
            break;

        case CF_LONG:
            AddCodeLine("ldy #$%04X", M_WORD(Offs + 2));
            AddCodeLine("lda (ptr1),y");
            AddCodeLine("ldy #$%04X", M_WORD(Offs));

            if (Flags & CF_TEST) {
                AddCodeLine("ora (ptr1),y");
            } else {
                AddCodeLine("sta sreg");
                AddCodeLine("lda (ptr1),y");
            }
            break;

        default:
            TypeError(Flags);
    }
}

void g_816_leasp(int StackOffs)
/* Adds code to load the address of a specified symbol. */
{
    AddCodeLine("tsc");
    AddCodeLine("clc");
    AddCodeLine("adc #$%04X", M_WORD(StackOffs + 1));
}

void g_816_leavariadic(unsigned ArgSizeOffs, int Offs)
/* Adds code to load the address of a variadic function's parameter. */
{
    AddCodeLine("tsc");
    AddCodeLine("clc");
    AddCodeLine("adc $%02X,s", M_BYTE(ArgSizeOffs + 1));

    if (Offs > 0) {
        g_816_inc(CF_INT | CF_CONST, Offs);
    } else if (Offs < 0) {
        g_816_dec(CF_INT | CF_CONST, -Offs);
    }
}

void g_816_putstatic(unsigned Flags, const char* LabelBuf)
/* Adds code to save a value to static memory. */
{
    switch (Flags & CF_TYPEMASK) {
        case CF_CHAR:
            AddCodeLine("sep #$20");
            AddCodeLine("sta %s", LabelBuf);
            AddCodeLine("rep #$20");
            break;

        case CF_INT:
            AddCodeLine("sta %s", LabelBuf);
            break;

        case CF_LONG:
            AddCodeLine("sta %s", LabelBuf);
            AddCodeLine("lda sreg");
            AddCodeLine("sta %s+2", LabelBuf);
            break;

        default:
            TypeError(Flags);
    }
}

void g_816_putlocal(unsigned Flags, int StackOffs, long Val)
/* Adds code to save a value inside of the stack. */
{
    switch (Flags & CF_TYPEMASK) {
        case CF_CHAR:
            AddCodeLine("sep #$20");
            if (Flags & CF_CONST) {
                AddCodeLine("lda #$%02X", M_BYTE(Val));
            }
            AddCodeLine("sta $%02X,s", M_BYTE(StackOffs + 1));
            AddCodeLine("rep #$20");
            break;

        case CF_INT:
            if (Flags & CF_CONST) {
                AddCodeLine("lda #$%04X", M_WORD(Val));
            }
            AddCodeLine("sta $%02X,s", M_BYTE(StackOffs + 1));
            break;

        case CF_LONG:
            if (Flags & CF_CONST) {
                AddCodeLine("lda #$%04X", M_WORD(Val));
                AddCodeLine("sta $%02X,s", M_BYTE(StackOffs + 1));
                AddCodeLine("lda #$%04X", M_WORD(Val >> 16));
                AddCodeLine("sta $%02X,s", M_BYTE(StackOffs + 3));
            } else {
                AddCodeLine("sta $%02X,s", M_BYTE(StackOffs + 1));
                AddCodeLine("lda sreg");
                AddCodeLine("sta $%02X,s", M_BYTE(StackOffs + 3));
            }
            break;

        default:
            TypeError(Flags);
    }
}

void g_816_putind(unsigned Flags, unsigned Offs)
/* Adds code to indirectly save a value to memory using an address on TOS. */
{
    AddCodeLine("ldy #$%04X", M_WORD(Offs));

    switch (Flags & CF_TYPEMASK) {
        case CF_CHAR:
            AddCodeLine("sep #$20");
            AddCodeLine("sta ($01,s),y");
            AddCodeLine("rep #$20");
            break;

        case CF_INT:
            AddCodeLine("sta ($01,s),y");
            break;

        case CF_LONG:
            AddCodeLine("sta ($01,s),y");
            AddCodeLine("ldy #$%04X", M_WORD(Offs + 2));
            AddCodeLine("lda sreg");
            AddCodeLine("sta ($01,s),y");
            break;

        default:
            TypeError(Flags);
    }
}

void g_816_toslong(unsigned Flags)
/* Adds code to ensure a value on TOS is a long. */
{
    switch (Flags & CF_TYPEMASK) {
        case CF_CHAR:
        case CF_INT:
            if (Flags & CF_UNSIGNED) {
                AddCodeLine("pla");
                AddCodeLine("pea $0000");
                AddCodeLine("pha");
            } else {
                unsigned PushZero = GetLocalLabel();
                unsigned EndSignExtension = GetLocalLabel();

                AddCodeLine("pla");
                AddCodeLine("bpl %s", LocalLabelName(PushZero));
                AddCodeLine("pea $FFFF");
                AddCodeLine("bra %s", LocalLabelName(EndSignExtension));
                g_defcodelabel(PushZero);
                AddCodeLine("pea $0000");
                g_defcodelabel(EndSignExtension);
                AddCodeLine("pha");
            }
            break;

        case CF_LONG:
            break;

        default:
            TypeError(Flags);
    }
}

void g_816_tosint(unsigned Flags)
/* Adds code to ensure a value on TOS is an int. */
{
    switch (Flags & CF_TYPEMASK) {
        case CF_CHAR:
        case CF_INT:
            break;

        case CF_LONG:
            /* Just lop off 16-bits (we want to preserve A's value, though). */
            AddCodeLine("tax");
            AddCodeLine("pla");
            AddCodeLine("sta $01,s");
            AddCodeLine("txa");
            break;

        default:
            TypeError(Flags);
    }
}

void g_816_regint(unsigned Flags)
/* Adds code to convert the current value in the primary register to an int. */
{
    /* Note that Flags hold what we are converting from. */
    switch (Flags & CF_TYPEMASK) {
        case CF_CHAR:
            if (Flags & CF_FORCECHAR) {
                AddCodeLine("and #$00FF");

                /* Handle sign extension if we need to. */
                if (!(Flags & CF_UNSIGNED)) {
                    unsigned EndSignExtension = GetLocalLabel();

                    AddCodeLine("bit #$0080");
                    AddCodeLine("beq %s", LocalLabelName(EndSignExtension));
                    AddCodeLine("ora #$FF00");
                    g_defcodelabel(EndSignExtension);
                }
            }
            break;

        case CF_INT:
        case CF_LONG:
            break;

        default:
            TypeError(Flags);
    }
}

void g_816_reglong(unsigned Flags)
/* Adds code to convert the current value in the primary register to a long. */
{
    /* Note that Flags hold what we are converting from. */
    switch (Flags & CF_TYPEMASK) {
        case CF_CHAR:
            AddCodeLine("stz sreg");

            /* Handle sign extension if we need to. */
            if (Flags & CF_UNSIGNED) {
                AddCodeLine("and #$00FF");
            } else {
                unsigned EndSignExtension = GetLocalLabel();

                AddCodeLine("bit #$0080");
                AddCodeLine("beq %s", LocalLabelName(EndSignExtension));
                AddCodeLine("ora #$FF00");
                AddCodeLine("dec sreg");
                g_defcodelabel(EndSignExtension);
            }
            break;

        case CF_INT:
            AddCodeLine("stz sreg");

            /* Handle sign extension if we need to. */
            if (!(Flags & CF_UNSIGNED)) {
                unsigned EndSignExtension = GetLocalLabel();

                AddCodeLine("bit #$8000");
                AddCodeLine("beq %s", LocalLabelName(EndSignExtension));
                AddCodeLine("dec sreg");
                g_defcodelabel(EndSignExtension);
            }
            break;

        case CF_LONG:
            break;

        default:
            TypeError(Flags);
    }
}

void g_816_addstatic(unsigned Flags, const char* LabelBuf)
/* Adds code to add a static variable to the primary register. */
{
    switch (Flags & CF_TYPEMASK) {
        case CF_CHAR:
            AddCodeLine("sep #$20");
            AddCodeLine("clc");
            AddCodeLine("adc %s", LabelBuf);
            AddCodeLine("rep #$20");
            break;

        case CF_INT:
            AddCodeLine("clc");
            AddCodeLine("adc %s", LabelBuf);
            break;

        case CF_LONG:
            AddCodeLine("clc");
            AddCodeLine("adc %s", LabelBuf);
            AddCodeLine("tax");
            AddCodeLine("lda sreg");
            AddCodeLine("adc %s+2", LabelBuf);
            AddCodeLine("sta sreg");
            AddCodeLine("txa");
            break;

        default:
            TypeError(Flags);
    }
}

void g_816_addlocal(unsigned Flags, int Offs)
/* Adds code to add a value from memory to the primary register. */
{
    unsigned EndCharAdd;

    switch (Flags & CF_TYPEMASK) {
        case CF_CHAR:
            AddCodeLine("sep #$20");
            AddCodeLine("clc");
            AddCodeLine("adc $%02X,s", M_BYTE(Offs + 1));
            AddCodeLine("rep #$20");
            break;

        case CF_INT:
            AddCodeLine("clc");
            AddCodeLine("adc $%02X,s", M_BYTE(Offs + 1));
            break;

        case CF_LONG:
            AddCodeLine("clc");
            AddCodeLine("adc $%02X,s", M_BYTE(Offs + 1));
            AddCodeLine("tax");
            AddCodeLine("lda sreg");
            AddCodeLine("adc $%02X,s", M_BYTE(Offs + 3));
            AddCodeLine("sta sreg");
            AddCodeLine("txa");
            break;

        default:
            TypeError(Flags);
    }
}

void g_816_addeqstatic(unsigned Flags, const char* LabelBuf, unsigned long Val)
/* Adds code to emit += for a static variable. */
{
    if ((Flags & CF_CONST) && (M_DWORD(Val) == 0)) {
        if (!(Flags & CF_NOKEEP)) {
            /* This is effectively a getstatic operation. */
            Flags &= ~(CF_FORCECHAR | CF_TEST);
            g_816_getstatic(Flags, LabelBuf);
        }

        return;
    }

    switch (Flags & CF_TYPEMASK) {
        case CF_CHAR:
            if (Flags & CF_FORCECHAR) {
                /* Prep the accumulator and switch to 8-bit mode. */
                if (Flags & CF_NOKEEP) {
                    AddCodeLine("sep #$20");
                    if ((Flags & CF_CONST) && (M_BYTE(Val) != 1)) {
                        AddCodeLine("lda #$%02X", M_BYTE(Val));
                    }
                } else {
                    if ((Flags & CF_CONST) && (M_BYTE(Val) != 1)) {
                        /* Combine loading the low & clearing the high bytes. */
                        AddCodeLine("lda #$00%02X", M_BYTE(Val));
                    } else {
                        AddCodeLine("and #$00FF");
                    }
                    AddCodeLine("sep #$20");
                }
                /* Do the actual math now... */
                if ((Flags & CF_CONST) && (M_BYTE(Val) == 1)) {
                    AddCodeLine("inc %s", LabelBuf);
                    if (!(Flags & CF_NOKEEP)) {
                        AddCodeLine("lda %s", LabelBuf);
                    }
                } else {
                    AddCodeLine("clc");
                    AddCodeLine("adc %s", LabelBuf);
                    AddCodeLine("sta %s", LabelBuf);
                }
                AddCodeLine("rep #$20");
                /* Handle sign extension if we need to. */
                if (!(Flags & CF_UNSIGNED) && !(Flags & CF_NOKEEP)) {
                    unsigned EndSignExtension = GetLocalLabel();
                    /* The n flag should be set in any case by this point. */
                    AddCodeLine("bpl %s", LocalLabelName(EndSignExtension));
                    AddCodeLine("ora #$FF00");
                    g_defcodelabel(EndSignExtension);
                }
                break;
            }
        /* Fall through... */
        case CF_INT:
            if ((Flags & CF_CONST) && (M_WORD(Val) == 1)) {
                AddCodeLine("inc %s", LabelBuf);
                if (!(Flags & CF_NOKEEP)) {
                    AddCodeLine("lda %s", LabelBuf);
                }
            } else {
                if (Flags & CF_CONST) {
                    AddCodeLine("lda #$%04X", M_WORD(Val));
                }
                AddCodeLine("clc");
                AddCodeLine("adc %s", LabelBuf);
                AddCodeLine("sta %s", LabelBuf);
            }
            break;

        case CF_LONG:
            AddCodeLine("clc");
            /* Low word... */
            if (!(Flags & CF_CONST) || (M_WORD(Val) != 0)) {
                if (Flags & CF_CONST) {
                    AddCodeLine("lda #$%04X", M_WORD(Val));
                }
                AddCodeLine("adc %s", LabelBuf);
                AddCodeLine("sta %s", LabelBuf);

                /* Skip the high word if we can. */
                if ((Flags & CF_CONST) && (M_WORD(Val >> 16) == 0 &&
                    (Flags & CF_NOKEEP))) {
                    break;
                } else if (!(Flags & CF_NOKEEP)) {
                    /* Save the low word if we need to. */
                    AddCodeLine("tax");
                }
            }
            /* High word... */
            if (Flags & CF_CONST) {
                AddCodeLine("lda #$%04X", M_WORD(Val >> 16));
            } else {
                AddCodeLine("lda sreg");
            }
            AddCodeLine("adc %s+2", LabelBuf);
            AddCodeLine("sta %s+2", LabelBuf);
            if (!(Flags & CF_NOKEEP)) {
                AddCodeLine("sta sreg");
                if (Flags & CF_CONST) {
                    AddCodeLine("lda #$%04X", M_WORD(Val));
                } else {
                    AddCodeLine("txa");
                }
            }
            break;

        default:
            TypeError(Flags);
    }
}

void g_816_addeqlocal(unsigned Flags, int StackOffs, unsigned long Val)
/* Adds code to emit += for a local variable. */
{
    if ((Flags & CF_CONST) && (M_DWORD(Val) == 0)) {
        if (!(Flags & CF_NOKEEP)) {
            /* This is effectively a getlocal operation. */
            Flags &= ~(CF_FORCECHAR | CF_TEST);
            g_816_getlocal(Flags, StackOffs);
        }

        return;
    }

    switch (Flags & CF_TYPEMASK) {
        case CF_CHAR:
            if (Flags & CF_FORCECHAR) {
                /* Prep the accumulator and switch to 8-bit mode. */
                if (Flags & CF_NOKEEP) {
                    AddCodeLine("sep #$20");
                    if (Flags & CF_CONST) {
                        AddCodeLine("lda #$%02X", M_BYTE(Val));
                    }
                } else {
                    if (Flags & CF_CONST) {
                        /* Combine loading the low & clearing the high bytes. */
                        AddCodeLine("lda #$00%02X", M_BYTE(Val));
                    } else {
                        AddCodeLine("and #$00FF");
                    }
                    AddCodeLine("sep #$20");
                }
                /* Do the actual math now... */
                AddCodeLine("clc");
                AddCodeLine("adc $%02X,s", M_BYTE(StackOffs + 1));
                AddCodeLine("sta $%02X,s", M_BYTE(StackOffs + 1));
                AddCodeLine("rep #$20");
                /* Handle sign extension if we need to. */
                if (!(Flags & CF_UNSIGNED) && !(Flags & CF_NOKEEP)) {
                    unsigned EndSignExtension = GetLocalLabel();

                    AddCodeLine("bpl %s", LocalLabelName(EndSignExtension));
                    AddCodeLine("ora #$FF00");
                    g_defcodelabel(EndSignExtension);
                }
                break;
            }
        /* Fall through... */
        case CF_INT:
            if (Flags & CF_CONST) {
                AddCodeLine("lda #$%04X", M_WORD(Val));
            }
            AddCodeLine("clc");
            AddCodeLine("adc $%02X,s", M_BYTE(StackOffs + 1));
            AddCodeLine("sta $%02X,s", M_BYTE(StackOffs + 1));
            break;

        case CF_LONG:
            AddCodeLine("clc");
            /* Low word... */
            if (!(Flags & CF_CONST) || (M_WORD(Val) != 0)) {
                if (Flags & CF_CONST) {
                    AddCodeLine("lda #$%04X", M_WORD(Val));
                }
                AddCodeLine("adc $%02X,s", M_BYTE(StackOffs + 1));
                AddCodeLine("sta $%02X,s", M_BYTE(StackOffs + 1));

                /* Skip the high word if we can. */
                if ((Flags & CF_CONST) && (M_WORD(Val >> 16) == 0 &&
                    (Flags & CF_NOKEEP))) {
                    break;
                } else if (!(Flags & CF_NOKEEP)) {
                    /* Save the low word if we need to. */
                    AddCodeLine("tax");
                }
            }
            /* High word... */
            if (Flags & CF_CONST) {
                AddCodeLine("lda #$%04X", M_WORD(Val >> 16));
            } else {
                AddCodeLine("lda sreg");
            }
            AddCodeLine("adc $%02X,s", M_BYTE(StackOffs + 3));
            AddCodeLine("sta $%02X,s", M_BYTE(StackOffs + 3));
            if (!(Flags & CF_NOKEEP)) {
                AddCodeLine("sta sreg");
                if (Flags & CF_CONST) {
                    AddCodeLine("lda #$%04X", M_WORD(Val));
                } else {
                    AddCodeLine("txa");
                }
            }
            break;

    default:
        TypeError(Flags);
    }
}

void g_816_addeqind(unsigned Flags, unsigned Offs, unsigned long Val)
/* Adds code to emit += for the address in the primary register. */
{
    if (M_DWORD(Val) == 0) {
        if (!(Flags & CF_NOKEEP)) {
            /* This is effectively a getind operation. */
            Flags &= ~(CF_FORCECHAR | CF_TEST);
            g_816_getind(Flags, Offs);
        }

        return;
    }

    AddCodeLine("sta ptr1");

    switch (Flags & CF_TYPEMASK) {
        case CF_CHAR:
            if (Flags & CF_FORCECHAR) {
                AddCodeLine("ldy #$%04X", M_WORD(Offs));

                /* Prep the accumulator and switch to 8-bit mode. */
                if (Flags & CF_NOKEEP) {
                    AddCodeLine("sep #$20");
                    AddCodeLine("lda #$%02X", M_BYTE(Val));
                } else {
                    /* Combine loading the low & clearing the high bytes. */
                    AddCodeLine("lda #$00%02X", M_BYTE(Val));
                    AddCodeLine("sep #$20");
                }
                /* Do the actual math now... */
                AddCodeLine("clc");
                AddCodeLine("adc (ptr1),y");
                AddCodeLine("sta (ptr1),y");
                AddCodeLine("rep #$20");
                /* Handle sign extension if we need to. */
                if (!(Flags & CF_UNSIGNED) && !(Flags & CF_NOKEEP)) {
                    unsigned EndSignExtension = GetLocalLabel();

                    AddCodeLine("bpl %s", LocalLabelName(EndSignExtension));
                    AddCodeLine("ora #$FF00");
                    g_defcodelabel(EndSignExtension);
                }
                break;
            }
        /* Fall through... */
        case CF_INT:
            AddCodeLine("ldy #$%04X", M_WORD(Offs));

            AddCodeLine("lda #$%04X", M_WORD(Val));
            AddCodeLine("clc");
            AddCodeLine("adc (ptr1),y");
            AddCodeLine("sta (ptr1),y");
            break;

        /* Recall that the DWORD isn't zero; at least one word is non-zero. */
        case CF_LONG:
            AddCodeLine("clc");
            /* Only do the low word if we have to. */
            if (M_WORD(Val) != 0) {
                AddCodeLine("ldy #$%04X", M_WORD(Offs));

                AddCodeLine("lda #$%04X", M_WORD(Val));
                AddCodeLine("adc (ptr1),y");
                AddCodeLine("sta (ptr1),y");

                /* We still need to consider the high word (carry). */
                if (!(Flags & CF_NOKEEP)) {
                    AddCodeLine("tax");
                }
                AddCodeLine("ldy #$%04X", M_WORD(Offs + 2));
                AddCodeLine("lda #$%04X", M_WORD(Val >> 16));
                AddCodeLine("adc (ptr1),y");
                AddCodeLine("sta (ptr1),y");
                if (!(Flags & CF_NOKEEP)) {
                    AddCodeLine("sta sreg");
                    AddCodeLine("txa");
                }
            } else {
                /* If we only have to do the high word, take shortcuts. */
                AddCodeLine("ldy #$%04X", M_WORD(Offs + 2));

                AddCodeLine("lda #$%04X", M_WORD(Val >> 16));
                AddCodeLine("adc (ptr1),y");
                AddCodeLine("sta (ptr1),y");
                if (!(Flags & CF_NOKEEP)) {
                    AddCodeLine("sta sreg");
                    AddCodeLine("ldy #$%04X", M_WORD(Offs));
                    AddCodeLine("lda (ptr1),y");
                }
            }
            break;

        default:
            TypeError(Flags);
    }
}

void g_816_subeqstatic(unsigned Flags, const char* LabelBuf, unsigned long Val)
/* Adds code to emit -= for a static variable. */
{
    if ((Flags & CF_CONST) && (M_DWORD(Val) == 0)) {
        if (!(Flags & CF_NOKEEP)) {
            /* This is effectively a getstatic operation. */
            Flags &= ~(CF_FORCECHAR | CF_TEST);
            g_816_getstatic(Flags, LabelBuf);
        }

        return;
    }

    switch (Flags & CF_TYPEMASK) {
        case CF_CHAR:
            if (Flags & CF_FORCECHAR) {
                /* Prep the accumulator and switch to 8-bit mode. */
                if (!(Flags & CF_NOKEEP)) {
                    AddCodeLine("and #$00FF");
                }
                AddCodeLine("sep #$20");
                /* Do the actual math now... */
                if (Flags & CF_CONST) {
                    if (M_BYTE(Val) == 1) {
                        AddCodeLine("dec %s", LabelBuf);
                        if (!(Flags & CF_NOKEEP)) {
                            AddCodeLine("lda %s", LabelBuf);
                        }
                    } else {
                        AddCodeLine("lda %s", LabelBuf);
                        AddCodeLine("sec");
                        AddCodeLine("sbc #$%02X", M_BYTE(Val));
                        AddCodeLine("sta %s", LabelBuf);
                    }
                } else {
                    AddCodeLine("eor #$FF");
                    AddCodeLine("sec");
                    AddCodeLine("adc %s", LabelBuf);
                    AddCodeLine("sta %s", LabelBuf);
                }
                AddCodeLine("rep #$20");
                /* Handle sign extension if we need to. */
                if (!(Flags & CF_UNSIGNED) && !(Flags & CF_NOKEEP)) {
                    unsigned EndSignExtension = GetLocalLabel();
                    /* The n flag should be set in any case by this point. */
                    AddCodeLine("bpl %s", LocalLabelName(EndSignExtension));
                    AddCodeLine("ora #$FF00");
                    g_defcodelabel(EndSignExtension);
                }
                break;
            }
        /* Fall through... */
        case CF_INT:
            if (Flags & CF_CONST) {
                if (M_WORD(Val) == 1) {
                    AddCodeLine("dec %s", LabelBuf);
                    if (!(Flags & CF_NOKEEP)) {
                        AddCodeLine("lda %s", LabelBuf);
                    }
                } else {
                    AddCodeLine("lda %s", LabelBuf);
                    AddCodeLine("sec");
                    AddCodeLine("sbc #$%04X", M_WORD(Val));
                    AddCodeLine("sta %s", LabelBuf);
                }
            } else {
                AddCodeLine("eor #$FFFF");
                AddCodeLine("sec");
                AddCodeLine("adc %s", LabelBuf);
                AddCodeLine("sta %s", LabelBuf);
            }
            break;

        /* Recall that the DWORD isn't zero; at least one word is non-zero. */
        case CF_LONG:
            AddCodeLine("sec");
            /* Low word... */
            if (!(Flags & CF_CONST) || (M_WORD(Val) != 0)) {
                if (Flags & CF_CONST) {
                    AddCodeLine("lda %s", LabelBuf);
                    AddCodeLine("sbc #$%04X", M_WORD(Val));
                    AddCodeLine("sta %s", LabelBuf);
                } else {
                    AddCodeLine("eor #$FFFF");
                    AddCodeLine("adc %s", LabelBuf);
                    AddCodeLine("sta %s", LabelBuf);
                }

                /* Skip the high word if we can. */
                if ((Flags & CF_CONST) && (M_WORD(Val >> 16) == 0) &&
                    (Flags & CF_NOKEEP)) {
                    break;
                } else if (!(Flags & CF_NOKEEP)) {
                    /* Save the low word if we need to. */
                    AddCodeLine("tay");
                }
            }
            /* High word... */
            if (Flags & CF_CONST) {
                AddCodeLine("lda %s+2", LabelBuf);
                AddCodeLine("sbc #$%04X", M_WORD(Val >> 16));
                AddCodeLine("sta %s+2", LabelBuf);
            } else {
                AddCodeLine("lda sreg");
                AddCodeLine("eor #$FFFF");
                AddCodeLine("adc %s+2", LabelBuf);
                AddCodeLine("sta %s+2", LabelBuf);
            }
            if (!(Flags & CF_NOKEEP)) {
                AddCodeLine("sta sreg");
                if (Flags & CF_CONST) {
                    AddCodeLine("lda #$%04X", M_WORD(Val));
                } else {
                    AddCodeLine("tya");
                }
            }
            break;

        default:
            TypeError(Flags);
    }
}

void g_816_subeqlocal(unsigned Flags, int StackOffs, unsigned long Val)
/* Adds code to emit -= for a local variable. */
{
    if ((Flags & CF_CONST) && (M_DWORD(Val) == 0)) {
        if (!(Flags & CF_NOKEEP)) {
            /* This is effectively a getlocal operation. */
            Flags &= ~(CF_FORCECHAR | CF_TEST);
            g_816_getlocal(Flags, StackOffs);
        }

        return;
    }

    switch (Flags & CF_TYPEMASK) {
        case CF_CHAR:
            if (Flags & CF_FORCECHAR) {
                /* Prep the accumulator and switch to 8-bit mode. */
                if (!(Flags & CF_NOKEEP)) {
                    AddCodeLine("and #$00FF");
                }
                AddCodeLine("sep #$20");
                /* Do the actual math now... */
                if (Flags & CF_CONST) {
                    AddCodeLine("lda $%02,s", M_BYTE(StackOffs + 1));
                    AddCodeLine("sec");
                    AddCodeLine("sbc #$%02X", M_BYTE(Val));
                    AddCodeLine("sta $%02,s", M_BYTE(StackOffs + 1));
                } else {
                    AddCodeLine("eor #$FF");
                    AddCodeLine("sec");
                    AddCodeLine("adc $%02X,s", M_BYTE(StackOffs + 1));
                    AddCodeLine("sta $%02X,s", M_BYTE(StackOffs + 1));
                }
                AddCodeLine("rep #$20");
                /* Handle sign extension if we need to. */
                if (!(Flags & CF_UNSIGNED) && !(Flags & CF_NOKEEP)) {
                    unsigned EndSignExtension = GetLocalLabel();
                    /* The n flag should be set in any case by this point. */
                    AddCodeLine("bpl %s", LocalLabelName(EndSignExtension));
                    AddCodeLine("ora #$FF00");
                    g_defcodelabel(EndSignExtension);
                }
                break;
            }
        /* Fall through... */
        case CF_INT:
            if (Flags & CF_CONST) {
                AddCodeLine("lda $%02X,s", M_BYTE(StackOffs + 1));
                AddCodeLine("sec");
                AddCodeLine("sbc #$%04X", M_WORD(Val));
                AddCodeLine("sta $%02X,s", M_BYTE(StackOffs + 1));
            } else {
                AddCodeLine("eor #$FFFF");
                AddCodeLine("sec");
                AddCodeLine("adc $%02X,s", M_BYTE(StackOffs + 1));
                AddCodeLine("sta $%02X,s", M_BYTE(StackOffs + 1));
            }
            break;

        /* Recall that the DWORD isn't zero; at least one word is non-zero. */
        case CF_LONG:
            AddCodeLine("sec");
            /* Low word... */
            if (!(Flags & CF_CONST) || (M_WORD(Val) != 0)) {
                if (Flags & CF_CONST) {
                    AddCodeLine("lda $%02X,s", M_BYTE(StackOffs + 1));
                    AddCodeLine("sbc #$%04X", M_WORD(Val));
                    AddCodeLine("sta $%02X,s", M_BYTE(StackOffs + 1));
                } else {
                    AddCodeLine("eor #$FFFF");
                    AddCodeLine("adc $%02X,s", M_BYTE(StackOffs + 1));
                    AddCodeLine("sta $%02X,s", M_BYTE(StackOffs + 1));
                }

                /* Skip the high word if we can. */
                if ((Flags & CF_CONST) && (M_WORD(Val >> 16) == 0) &&
                    (Flags & CF_NOKEEP)) {
                    break;
                } else if (!(Flags & CF_NOKEEP)) {
                    /* Save the low word if we need to. */
                    AddCodeLine("tay");
                }
            }
            /* High word... */
            if (Flags & CF_CONST) {
                AddCodeLine("lda $%02X,s", M_BYTE(StackOffs + 3));
                AddCodeLine("sbc #$%04X", M_WORD(Val >> 16));
                AddCodeLine("sta $%02X,s", M_BYTE(StackOffs + 3));
            } else {
                AddCodeLine("lda sreg");
                AddCodeLine("eor #$FFFF");
                AddCodeLine("adc $%02X,s", M_BYTE(StackOffs + 3));
                AddCodeLine("sta $%02X,s", M_BYTE(StackOffs + 3));
            }
            if (!(Flags & CF_NOKEEP)) {
                AddCodeLine("sta sreg");
                if (Flags & CF_CONST) {
                    AddCodeLine("lda #$%04X", M_WORD(Val));
                } else {
                    AddCodeLine("tya");
                    }
            }
            break;

        default:
            TypeError(Flags);
    }
}

void g_816_subeqind(unsigned Flags, unsigned Offs, unsigned long Val)
/* Adds code to emit -= for the address in the primary register. */
{
    if (M_DWORD(Val) == 0) {
        if (!(Flags & CF_NOKEEP)) {
            /* This is effectively a getind operation. */
            Flags &= ~(CF_FORCECHAR | CF_TEST);
            g_816_getind(Flags, Offs);
        }

        return;
    }

    AddCodeLine("sta ptr1");

    switch (Flags & CF_TYPEMASK) {
        case CF_CHAR:
            if (Flags & CF_FORCECHAR) {
                AddCodeLine("ldy #$%04X", M_WORD(Offs));

                /* Prep the accumulator and switch to 8-bit mode. */
                if (!(Flags & CF_NOKEEP)) {
                    AddCodeLine("and #$00FF");
                }
                AddCodeLine("sep #$20");
                /* Do the actual math now... */
                AddCodeLine("lda (ptr1),y");
                AddCodeLine("sec");
                AddCodeLine("sbc #$%02X", M_BYTE(Val));
                AddCodeLine("sta (ptr1),y");
                AddCodeLine("rep #$20");
                /* Handle sign extension if we need to. */
                if (!(Flags & CF_UNSIGNED) && !(Flags & CF_NOKEEP)) {
                    unsigned EndSignExtension = GetLocalLabel();

                    AddCodeLine("bpl %s", LocalLabelName(EndSignExtension));
                    AddCodeLine("ora #$FF00");
                    g_defcodelabel(EndSignExtension);
                }
                break;
            }
        /* Fall through... */
        case CF_INT:
            AddCodeLine("ldy #$%04X", M_WORD(Offs));

            AddCodeLine("lda (ptr1),y");
            AddCodeLine("sec");
            AddCodeLine("sbc #$%04X", M_WORD(Val));
            AddCodeLine("sta (ptr1),y");
            break;

        /* Recall that the DWORD isn't zero; at least one word is non-zero. */
        case CF_LONG:
            AddCodeLine("sec");
            /* Only do the low word if we have to. */
            if (M_WORD(Val) != 0) {
                AddCodeLine("ldy #$%04X", M_WORD(Offs));

                AddCodeLine("lda #$%04X", M_WORD(Val));
                AddCodeLine("sbc (ptr1),y");
                AddCodeLine("sta (ptr1),y");

                /* We still need to consider the high word (carry). */
                if (!(Flags & CF_NOKEEP)) {
                    AddCodeLine("tax");
                }
                AddCodeLine("ldy #$%04X", M_WORD(Offs + 2));
                AddCodeLine("lda #$%04X", M_WORD(Val >> 16));
                AddCodeLine("sbc (ptr1),y");
                AddCodeLine("sta (ptr1),y");
                if (!(Flags & CF_NOKEEP)) {
                    AddCodeLine("sta sreg");
                    AddCodeLine("txa");
                }
            } else {
                /* If we only have to do the high word, take shortcuts. */
                AddCodeLine("ldy #$%04X", M_WORD(Offs + 2));

                AddCodeLine("lda #$%04X", M_WORD(Val >> 16));
                AddCodeLine("sbc (ptr1),y");
                AddCodeLine("sta (ptr1),y");
                if (!(Flags & CF_NOKEEP)) {
                    AddCodeLine("sta sreg");
                    AddCodeLine("ldy #$%04X", M_WORD(Offs));
                    AddCodeLine("lda (ptr1),y");
                }
            }
            break;

        default:
            TypeError(Flags);
    }
}

void g_816_addaddr_local(int StackOffs)
/* Adds code to add a local variable's address to the primary register. */
{
    AddCodeLine("tsx");
    AddCodeLine("stx sp");
    AddCodeLine("clc");
    AddCodeLine("adc sp");
    AddCodeLine("clc");
    AddCodeLine("adc #$%04X", M_WORD(StackOffs + 1));
}

void g_816_addaddr_static(const char* LabelBuf)
/* Adds code to add a static variable's address to the primary register. */
{
    AddCodeLine("clc");
    AddCodeLine("adc #.loword(%s)", LabelBuf);
}

void g_816_save(unsigned Flags)
/* Adds code to save the primary register to the hold register. */
{
    switch (Flags & CF_TYPEMASK) {
        case CF_CHAR:
            if (Flags & CF_FORCECHAR) {
                AddCodeLine("sep #$20");
                AddCodeLine("sta regsave");
                AddCodeLine("rep #$20");
                break;
            }
        /* Fall through... */
        case CF_INT:
            AddCodeLine("sta regsave");
            break;

        case CF_LONG:
            AddCodeLine("sta regsave");
            AddCodeLine("ldx sreg");
            AddCodeLine("stx regsave+2");
            break;

        default:
            TypeError(Flags);
    }
}

void g_816_restore(unsigned Flags)
/* Adds code to restore the hold register to the primary register. */
{
    switch (Flags & CF_TYPEMASK) {
        case CF_CHAR:
            if (Flags & CF_FORCECHAR) {
                AddCodeLine("sep #$20");
                AddCodeLine("lda regsave");
                AddCodeLine("rep #$20");
                break;
            }
        /* Fall through... */
        case CF_INT:
            AddCodeLine("lda regsave");
            break;

        case CF_LONG:
            AddCodeLine("lda regsave");
            AddCodeLine("ldx regsave+2");
            AddCodeLine("stx sreg");
            break;

        default:
            TypeError(Flags);
    }
}

void g_816_cmp(unsigned Flags, unsigned long Val)
/* Adds code to perform an immediate comparison (primary reg unchanged). */
{
    unsigned EndLongCmp;

    switch (Flags & CF_TYPEMASK) {
        case CF_CHAR:
            if (Flags & CF_FORCECHAR) {
                AddCodeLine("sep #$20");
                AddCodeLine("cmp #$%02X", M_BYTE(Val));
                AddCodeLine("rep #$20");
                break;
            }

        case CF_INT:
            AddCodeLine("cmp #$%04X", M_WORD(Val));
            break;

        case CF_LONG:
            EndLongCmp = GetLocalLabel();

            AddCodeLine("cmp #$%04X", M_WORD(Val));
            AddCodeLine("bne %s", LocalLabelName(EndLongCmp));
            AddCodeLine("ldx sreg");
            AddCodeLine("cpx #$%04X", M_WORD(Val >> 16));
            g_defcodelabel(EndLongCmp);
            break;

        default:
            TypeError(Flags);
    }
}

void g_816_test(unsigned Flags)
/* Adds code to test the value in the primary register. */
{
    switch (Flags & CF_TYPEMASK) {
        case CF_CHAR:
            AddCodeLine("bit #$00FF");
            break;

        case CF_INT:
            AddCodeLine("tax");
            break;

        case CF_LONG:
            if (Flags & CF_NOKEEP) {
                AddCodeLine("ora sreg");
            } else {
                AddCodeLine("tax");
                AddCodeLine("ora sreg");
                AddCodeLine("txa");
            }
            break;

        default:
            TypeError(Flags);
    }
}

void g_816_push(unsigned Flags, unsigned long Val)
/* Adds code to push the primary register's value or a constant to the stack. */
{
    switch (Flags & CF_TYPEMASK) {
        case CF_CHAR:
            if (Flags & CF_FORCECHAR) {
                AddCodeLine("sep #$20");
                if (Flags & CF_CONST) {
                    AddCodeLine("lda #$%02X", M_BYTE(Val));
                }
                AddCodeLine("pha");
                AddCodeLine("rep #$20");
                break;
            }
        /* Fall through... */
        case CF_INT:
            if (Flags & CF_CONST) {
                AddCodeLine("pea $%04X", M_WORD(Val));
            } else {
                AddCodeLine("pha");
            }
            break;

        case CF_LONG:
            if (Flags & CF_CONST) {
                AddCodeLine("pea $%04X", M_WORD(Val >> 16));
                AddCodeLine("pea $%04X", M_WORD(Val));
            } else {
                AddCodeLine("ldx sreg");
                AddCodeLine("phx");
                AddCodeLine("pha");
            }
            break;

        default:
            TypeError(Flags);
    }
}

void g_816_swap(unsigned Flags)
/* Adds code to swap the top of the stack and the primary register. */
{
    switch (Flags & CF_TYPEMASK) {
        case CF_CHAR:
            if (Flags & CF_FORCECHAR) {
                AddCodeLine("sep #$20");
                AddCodeLine("plx");
                AddCodeLine("pha");
                AddCodeLine("txa");
                AddCodeLine("rep #$20");
                break;
            }
        /* Fall through... */
        case CF_INT:
            AddCodeLine("plx");
            AddCodeLine("pha");
            AddCodeLine("txa");
            break;

        case CF_LONG:
            AddCodeLine("plx");
            AddCodeLine("pha");
            AddCodeLine("lda $03,s");
            AddCodeLine("tay");
            AddCodeLine("lda sreg");
            AddCodeLine("sta $03,s");
            AddCodeLine("sty sreg");
            AddCodeLine("txa");
            break;

        default:
            TypeError(Flags);
    }
}

void g_816_callind(unsigned Flags, unsigned ArgSize, int StackOffs)
/* Adds code to call a subroutine indirectly. */
{
    unsigned ReturnLabel = GetLocalLabel();

    if ((Flags & CF_ADDRMASK) == CF_STACK) {
        /* Address to call on the stack. */
        AddCodeLine("tay");
        AddCodeLine("lda $%02X,s", M_BYTE(StackOffs + 1));
        /* Remember to compensate for PC incrementing on return. */
        AddCodeLine("pea %s-1", LocalLabelName(ReturnLabel));
        AddCodeLine("pha");
        AddCodeLine("tya");
    } else {
        /* Address to call in A. */
        /* Remember to compensate for PC incrementing on return. */
        AddCodeLine("pea %s-1", LocalLabelName(ReturnLabel));
        AddCodeLine("dec");
        AddCodeLine("pha");
        
        if (!(Flags & CF_FIXARGC)) {
            /* Pass arg count through Y. */
            AddCodeLine("ldy #$%04X", M_WORD(ArgSize));
        }
    }

    AddCodeLine("rts");

    g_defcodelabel(ReturnLabel);
}

void g_816_lateadjustsp(unsigned Label)
/* Adds code to adjust the stack pointer pased on non-immediate data. */
{
    /* Use sp as a buffer so we can work with the stack. */
    AddCodeLine("sta sp");
    AddCodeLine("tsc");
    AddCodeLine("clc");
    AddCodeLine("adc %s", LocalDataLabelName(Label));
    AddCodeLine("tcs");
    AddCodeLine("lda sp");
}

void g_816_drop(unsigned Space)
/* Adds code to drop space on the stack. */
{
    /* Use sp as a buffer so we can work with the stack. */
    AddCodeLine("sta sp");
    AddCodeLine("tsc");
    AddCodeLine("clc");
    AddCodeLine("adc #$%04X", Space & 0xFFFF);
    AddCodeLine("tcs");
    AddCodeLine("lda sp");
}

void g_816_space(unsigned Space)
/* Adds code to create space on the stack. */
{
    /* Use sp as a buffer so we can work with the stack. */
    AddCodeLine("sta sp");
    AddCodeLine("tsc");
    AddCodeLine("clc");
    AddCodeLine("sbc #$%04X", Space & 0xFFFF);
    AddCodeLine("tcs");
    AddCodeLine("lda sp");
}

void g_816_add(unsigned Flags, unsigned long Val)
/* Adds code to add the TOS to the primary register. */
{
    switch (Flags & CF_TYPEMASK) {
        case CF_CHAR:
            if (Flags & CF_FORCECHAR) {
                AddCodeLine("sep #$20");
                if (Flags & CF_CONST) {
                    AddCodeLine("lda #$%02X", M_BYTE(Val));
                }
                AddCodeLine("clc");
                AddCodeLine("adc $01,s");
                AddCodeLine("rep #$20");
                break;
            }
        /* Fall through... */
        case CF_INT:
            if (Flags & CF_CONST) {
                AddCodeLine("lda #$%04X", M_WORD(Val));
            }
            AddCodeLine("clc");
            AddCodeLine("adc $01,s");
            break;

        case CF_LONG:
            if (Flags & CF_CONST) {
                AddCodeLine("lda #$%04X", M_WORD(Val));
            }
            AddCodeLine("clc");
            AddCodeLine("adc $01,s");
            AddCodeLine("pha");
            if (Flags & CF_CONST) {
                AddCodeLine("lda #$%04X", M_WORD(Val >> 16));
            } else {
                AddCodeLine("lda sreg");
            }
            AddCodeLine("adc $05,s");
            AddCodeLine("sta sreg");
            AddCodeLine("pla");
            break;

        default:
            TypeError(Flags);
    }
}

void g_816_sub(unsigned Flags, unsigned long Val)
/* Adds code to subtract the primary register from the TOS. */
{
    switch (Flags & CF_TYPEMASK) {
        case CF_CHAR:
            if (Flags & CF_FORCECHAR) {
                AddCodeLine("sep #$20");
                if (Flags & CF_CONST) {
                    AddCodeLine("lda #$%02X", M_BYTE(Val));
                }
                AddCodeLine("sec");
                AddCodeLine("eor #$FF");
                AddCodeLine("adc $01,s");
                AddCodeLine("rep #$20");
                break;
            }
        /* Fall through... */
        case CF_INT:
            if (Flags & CF_CONST) {
                AddCodeLine("lda #$%04X", M_WORD(Val));
            }
            AddCodeLine("sec");
            AddCodeLine("eor #$FFFF");
            AddCodeLine("adc $01,s");
            break;

        case CF_LONG:
            if (Flags & CF_CONST) {
                AddCodeLine("lda #$%04X", M_WORD(Val));
            }
            AddCodeLine("sec");
            AddCodeLine("eor #$FFFF");
            AddCodeLine("adc $01,s");
            AddCodeLine("pha");
            if (Flags & CF_CONST) {
                AddCodeLine("lda #$%04X", M_WORD(Val >> 16));
            } else {
                AddCodeLine("lda sreg");
            }
            AddCodeLine("eor #$FFFF");
            AddCodeLine("adc $05,s");
            AddCodeLine("sta sreg");
            AddCodeLine("pla");
            break;

        default:
            TypeError(Flags);
    }
}

void g_816_rsub(unsigned Flags, unsigned long Val)
/* Adds code to subtract the TOS to the primary register. */
{
    switch (Flags & CF_TYPEMASK) {
        case CF_CHAR:
            if (Flags & CF_FORCECHAR) {
                AddCodeLine("sep #$20");
                if (Flags & CF_CONST) {
                    AddCodeLine("lda #$%02X", M_BYTE(Val));
                }
                AddCodeLine("sec");
                AddCodeLine("sbc $01,s");
                AddCodeLine("rep #$20");
                break;
            }
        /* Fall through... */
        case CF_INT:
            if (Flags & CF_CONST) {
                AddCodeLine("lda #$%04X", M_WORD(Val));
            }
            AddCodeLine("sec");
            AddCodeLine("sbc $01,s");
            break;

        case CF_LONG:
            if (Flags & CF_CONST) {
                AddCodeLine("lda #$%04X", M_WORD(Val));
            }
            AddCodeLine("sec");
            AddCodeLine("sbc $01,s");
            AddCodeLine("pha");
            if (Flags & CF_CONST) {
                AddCodeLine("lda #$%04X", M_WORD(Val >> 16));
            } else {
                AddCodeLine("lda sreg");
            }
            AddCodeLine("sbc $05,s");
            AddCodeLine("sta sreg");
            AddCodeLine("pla");
            break;

        default:
            TypeError(Flags);
    }
}

void g_816_mul(unsigned Flags, unsigned long Val)
/* Adds code to multiply the primary register with the TOS. */
{
    /* See if we need to push a constant value to the TOS... */
    if (Flags & CF_CONST) { g_816_push(Flags, Val); }
    /* Note: We presume the callee pops the argument passed. */

    switch (Flags & CF_TYPEMASK) {
        case CF_CHAR:
            if (Flags & CF_FORCECHAR) {
                AddCodeLine("jsr tosmul816_ubyte");
                break;
            }
        /* Fall through... */
        case CF_INT:
            AddCodeLine("jsr tosmul816_uword");
            break;

        case CF_LONG:
            AddCodeLine("jsr tosmul816_ulong");
            break;

        default:
            TypeError(Flags);
    }
}

void g_816_div(unsigned Flags, unsigned long Val)
/* Adds code to divide the TOS by the primary register. */
{
    /* See if we need to push a constant value to the TOS... */
    if (Flags & CF_CONST) { g_816_push(Flags, Val); }
    /* Note: We presume the callee pops the argument passed. */

    switch (Flags & CF_TYPEMASK) {
        case CF_CHAR:
            if (Flags & CF_FORCECHAR) {
                if (Flags & CF_UNSIGNED) {
                    AddCodeLine("jsr tosdiv816_ubyte");
                } else {
                    AddCodeLine("jsr tosdiv816_sbyte");
                }
                break;
            }
        /* Fall through... */
        case CF_INT:
            if (Flags & CF_UNSIGNED) {
                AddCodeLine("jsr tosdiv816_uword");
            } else {
                AddCodeLine("jsr tosdiv816_sword");
            }
            break;

        case CF_LONG:
            if (Flags & CF_UNSIGNED) {
                AddCodeLine("jsr tosdiv816_ulong");
            } else {
                AddCodeLine("jsr tosdiv816_slong");
            }
            break;

        default:
            TypeError(Flags);
    }
}

void g_816_mod(unsigned Flags, unsigned long Val)
/* Adds code to modulate the TOS by the primary register. */
{
    g_816_div(Flags, Val); /* Perform the division and load the remainder. */

    switch (Flags & CF_TYPEMASK) {
        case CF_CHAR:
        case CF_INT:
            AddCodeLine("txa");
            break;

        case CF_LONG:
            AddCodeLine("lda ptr4");
            AddCodeLine("sta sreg");
            AddCodeLine("lda ptr3");
            break;

        default:
            TypeError(Flags);
    }
}

void g_816_or(unsigned Flags, unsigned long Val)
/* Adds code to perform a bitwise or with the TOS and the primary register. */
{
    /* If const, then the lhs is in the primary register, not the stack, and
    ** the rhs is the value that we can apply as a constant. */
    switch (Flags & CF_TYPEMASK) {
        case CF_CHAR:
            if (Flags & CF_FORCECHAR) {
                AddCodeLine("sep #$20");
                if (Flags & CF_CONST) {
                    AddCodeLine("ora #$%02X", M_BYTE(Val));
                } else {
                    AddCodeLine("ora $01,s");
                }
                AddCodeLine("rep #$20");
                break;
            }
        /* Fall through... */
        case CF_INT:
            if (Flags & CF_CONST) {
                AddCodeLine("ora #$%04X", M_WORD(Val));
            } else {
                AddCodeLine("ora $01,s");
            }
            break;

        case CF_LONG:
            if (Flags & CF_CONST) {
                AddCodeLine("ora #$%04X", M_WORD(Val));
            } else {
                AddCodeLine("ora $01,s");
            }
            AddCodeLine("tax");
            AddCodeLine("lda sreg");
            if (Flags & CF_CONST) {
                AddCodeLine("ora #$%04X", M_WORD(Val >> 16));
            } else {
                AddCodeLine("ora $03,s");
            }
            AddCodeLine("sta sreg");
            AddCodeLine("txa");
            break;

        default:
            TypeError(Flags);
    }
}

void g_816_xor(unsigned Flags, unsigned long Val)
/* Adds code to perform a bitwise xor with the TOS and the primary register. */
{
    /* If const, then the lhs is in the primary register, not the stack, and
    ** the rhs is the value that we can apply as a constant. */
    switch (Flags & CF_TYPEMASK) {
        case CF_CHAR:
            if (Flags & CF_FORCECHAR) {
                AddCodeLine("sep #$20");
                if (Flags & CF_CONST) {
                    AddCodeLine("eor #$%02X", M_BYTE(Val));
                } else {
                    AddCodeLine("eor $01,s");
                }
                AddCodeLine("rep #20");
                break;
            }
        /* Fall through... */
        case CF_INT:
            if (Flags & CF_INT) {
                AddCodeLine("eor #$%04X", M_WORD(Val));
            } else {
                AddCodeLine("eor $01,s");
            }
            break;

        case CF_LONG:
            if (Flags & CF_CONST) {
                AddCodeLine("eor #$%04X", M_WORD(Val));
            } else {
                AddCodeLine("eor $01,s");
            }
            AddCodeLine("tax");
            AddCodeLine("lda sreg");
            if (Flags & CF_CONST) {
                AddCodeLine("eor #$%04X", M_WORD(Val >> 16));
            } else {
                AddCodeLine("eor $03,s");
            }
            AddCodeLine("sta sreg");
            AddCodeLine("txa");
            break;

        default:
            TypeError(Flags);
    }
}

void g_816_and(unsigned Flags, unsigned long Val)
/* Adds code to perform a bitwise and with the TOS and the primary register. */
{
    /* If const, then the lhs is in the primary register, not the stack, and
    ** the rhs is the value that we can apply as a constant. */
    switch (Flags & CF_TYPEMASK) {
    case CF_CHAR:
        if (Flags & CF_FORCECHAR) {
            AddCodeLine("sep #$20");
            if (Flags & CF_CONST) {
                AddCodeLine("and #$%02X", M_BYTE(Val));
            } else {
                AddCodeLine("and $01,s");
            }
            AddCodeLine("rep #20");
            break;
        }
        /* Fall through... */
    case CF_INT:
        if (Flags & CF_INT) {
            AddCodeLine("and #$%04X", M_WORD(Val));
        } else {
            AddCodeLine("and $01,s");
        }
        break;

    case CF_LONG:
        if (Flags & CF_CONST) {
            AddCodeLine("and #$%04X", M_WORD(Val));
        } else {
            AddCodeLine("and $01,s");
        }
        AddCodeLine("tax");
        AddCodeLine("lda sreg");
        if (Flags & CF_CONST) {
            AddCodeLine("and #$%04X", M_WORD(Val >> 16));
        } else {
            AddCodeLine("and $03,s");
        }
        AddCodeLine("sta sreg");
        AddCodeLine("txa");
        break;

    default:
        TypeError(Flags);
    }
}

void g_816_lsr(unsigned Flags, unsigned long Val)
/* Adds code to log. shift TOS right by the value in the primary register. */
{
    unsigned long ToShift = 0;
    int ShiftHiWord = 0;

    /* If const, then the lhs is in the primary register, not the stack, and
    ** the rhs is the value that we can apply as a constant. */
    switch (Flags & CF_TYPEMASK) {
        case CF_CHAR:
            if (Flags & CF_FORCECHAR) {
                if (!(Flags & CF_CONST)) {
                    AddCodeLine("jsr toslsr816_byte");
                    return;
                } else if (Val >= 8) {
                    AddCodeLine("and #$FF00");
                    return;
                } else {
                    ToShift = Val;
                }
                break;
            }
        /* Fall through... */
        case CF_INT:
            if (!(Flags & CF_CONST)) {
                AddCodeLine("jsr toslsr816_word");
                return;
            } else if (Val >= 16) {
                AddCodeLine("lda #$0000");
                return;
            } else {
                ToShift = Val;
            }
            break;

        case CF_LONG:
            if (!(Flags & CF_CONST)) {
                AddCodeLine("jsr toslsr816_long");
                return;
            } else if (Val >= 32) {
                AddCodeLine("stz sreg");
                AddCodeLine("lda #$0000");
                return;
            } else {
                ToShift = Val;
                ShiftHiWord = 1;
            }
            break;

        default:
            TypeError(Flags);
            return; /* Don't proceed with bad types. */
    }
    /* If we get past here, we need to take steps to shift. */

    if (ToShift >= 16) {
        AddCodeLine("lda sreg");
        AddCodeLine("stz sreg");

        ToShift -= 16;
        ShiftHiWord = 0;
    }
    if (ToShift >= 8) {
        if (ShiftHiWord) {
            AddCodeLine("sep #$20");
            AddCodeLine("lda sreg");
            AddCodeLine("xba");
            AddCodeLine("rep #$20");
            AddCodeLine("tax");
            AddCodeLine("lda sreg");
            AddCodeLine("xba");
            AddCodeLine("and #$00FF");
            AddCodeLine("sta sreg");
            AddCodeLine("txa");
        } else {
            AddCodeLine("xba");
            AddCodeLine("and #$00FF");
        }

        ToShift -= 8;
    }
    if (ShiftHiWord) {
        while (ToShift > 0) {
            AddCodeLine("lsr sreg");
            AddCodeLine("ror");

            ToShift--;
        }
    } else {
        while (ToShift > 0) {
            AddCodeLine("lsr");

            ToShift--;
        }
    }
}

void g_816_asr(unsigned Flags, unsigned long Val)
/* Adds code to arith. shift TOS right by the value in the primary register. */
{
    unsigned DoneLabel = GetLocalLabel();
    unsigned MaskLabel = GetLocalLabel();

    /* If const, then the lhs is in the primary register, not the stack, and
    ** the rhs is the value that we can apply as a constant. */
    switch (Flags & CF_TYPEMASK) {
        case CF_CHAR:
            if (Flags & CF_FORCECHAR) {
                if (!(Flags & CF_CONST)) {
                    AddCodeLine("jsr tosasr816_byte");
                } else if (Val >= 8) {
                    AddCodeLine("bit #$0080");
                    AddCodeLine("bne %s", LocalLabelName(MaskLabel));
                    AddCodeLine("and #$FF00");
                    AddCodeLine("bra %s", LocalLabelName(DoneLabel));
                    g_defcodelabel(MaskLabel);
                    AddCodeLine("ora #$00FF");
                    g_defcodelabel(DoneLabel);
                }  else {
                    /* Calling lsr and masking is less efficient, but practical. */
                    g_816_lsr(Flags, Val);
                    if (Val == 0) { return; }
                    /* Check if the 'last' bit of information is one - if so we mask. */
                    AddCodeLine("bit #$%04X", 0x80 >> Val);
                    AddCodeLine("beq %s", LocalLabelName(DoneLabel));
                    AddCodeLine("ora #$%04X", M_BYTE(0xFF << (8 - Val)));
                    g_defcodelabel(DoneLabel);
                }
                break;
            }
        /* Fall through... */
        case CF_INT:
            if (!(Flags & CF_CONST)) {
                AddCodeLine("jsr tosasr816_word");
            } else if (Val >= 16) {
                AddCodeLine("bit #$0080");
                AddCodeLine("bne %s", LocalLabelName(MaskLabel));
                AddCodeLine("lda #$0000");
                AddCodeLine("bra %s", LocalLabelName(DoneLabel));
                g_defcodelabel(MaskLabel);
                AddCodeLine("lda #$FFFF");
                g_defcodelabel(DoneLabel);
            } else {
                /* Calling lsr and masking is less efficient, but practical. */
                g_816_lsr(Flags, Val);
                if (Val == 0) { return; }
                /* Check if the 'last' bit of information is one - if so we mask. */
                AddCodeLine("bit #$%04X", 0x8000 >> Val);
                AddCodeLine("beq %s", LocalLabelName(DoneLabel));
                AddCodeLine("ora #$%04X", M_WORD(0xFFFF << (16 - Val)));
                g_defcodelabel(DoneLabel);
            }
            break;

        case CF_LONG:
            if (!(Flags & CF_CONST)) {
                AddCodeLine("jsr tosasr816_long");
            } else if (Val >= 32) {
                AddCodeLine("lda sreg");
                AddCodeLine("bmi %s", LocalLabelName(MaskLabel));
                AddCodeLine("lda #$0000");
                AddCodeLine("stz sreg");
                AddCodeLine("bra %s", LocalLabelName(DoneLabel));
                g_defcodelabel(MaskLabel);
                AddCodeLine("lda #$FFFF");
                AddCodeLine("sta sreg");
                g_defcodelabel(DoneLabel);
            } else {
                /* Calling lsr and masking is less efficient, but practical. */
                g_816_lsr(Flags, Val);
                if (Val == 0) { return; }
                /* Check if the 'last' bit of information is one - if so we mask. */
                if (Val >= 16) {
                    AddCodeLine("bit #$%04X", 0x8000 >> (Val - 16));
                    AddCodeLine("beq %s", LocalLabelName(DoneLabel));
                    if (Val != 16) {
                        AddCodeLine("ora #$%04X", M_WORD(0xFFFF << (32 - Val)));
                    }
                    AddCodeLine("dec sreg");
                } else {
                    AddCodeLine("ldx #$%04X", 0x8000 >> Val);
                    AddCodeLine("cpx sreg");
                    AddCodeLine("bcs %s", LocalLabelName(DoneLabel));
                    AddCodeLine("tax");
                    AddCodeLine("lda sreg");
                    AddCodeLine("ora #$%04X", M_WORD(0xFFFF << (16 - Val)));
                    AddCodeLine("sta sreg");
                    AddCodeLine("txa");
                }
                g_defcodelabel(DoneLabel);
            }
            break;

        default:
            TypeError(Flags);
    }
}

void g_816_asl(unsigned Flags, unsigned long Val)
/* Adds code to arith. shift TOS left by the value in the primary register. */
{
    unsigned long ToShift = 0;
    int ShiftHiWord = 0;

    /* If const, then the lhs is in the primary register, not the stack, and
    ** the rhs is the value that we can apply as a constant. */
    switch (Flags & CF_TYPEMASK) {
        case CF_CHAR:
            if (Flags & CF_FORCECHAR) {
                if (!(Flags & CF_CONST)) {
                    AddCodeLine("jsr tosasl816_byte");
                } else if (Val >= 8) {
                    AddCodeLine("and #$FF00");
                } else {
                    ToShift = Val;
                }
                break;
            }
        /* Fall through... */
        case CF_INT:
            if (!(Flags & CF_CONST)) {
                AddCodeLine("jsr tosasl816_word");
            } else if (Val >= 16) {
                AddCodeLine("lda #$0000");
            } else {
                ToShift = Val;
            }
            break;

        case CF_LONG:
            if (!(Flags & CF_CONST)) {
                AddCodeLine("jsr tosasl816_long");
            } else if (Val >= 32) {
                AddCodeLine("stz sreg");
                AddCodeLine("lda #$0000");
            } else {
                ToShift = Val;
                ShiftHiWord = 1;
            }
            break;

        default:
            TypeError(Flags);
            return; /* Don't proceed with bad types. */
    }
    /* If we get past here, we need to take steps to shift. */

    if (!ShiftHiWord || (ToShift >= 16)) {
        if (ToShift >= 16) {
            /* Don't bother with the high word shifting (we'll wipe it). */
            ToShift -= 16;
        }

        if (ToShift >= 8) {
            AddCodeLine("xba");
            AddCodeLine("and #$FF00");

            ToShift -= 8;
        }

        while (ToShift > 0) {
            AddCodeLine("asl");

            ToShift--;
        }

        if (ToShift >= 16) {
            /* Finally make good on shifting over by a word. */
            AddCodeLine("sta sreg");
            AddCodeLine("lda #$0000");
        }
    } else {
        /* We still need to shift the high word's contents. */
        if (ToShift >= 8) {
            AddCodeLine("xba");
            AddCodeLine("sep #$20");
            AddCodeLine("sta ptr3+1"); /* Keep our shifted low word in ptr3. */
            AddCodeLine("stz ptr3");
            AddCodeLine("lda sreg");
            AddCodeLine("rep #$20");
            AddCodeLine("xba"); /* Now we have our high word in A. */
            AddCodeLine("sta sreg");

            ToShift -= 8;
        }

        while (ToShift > 0) {
            AddCodeLine("asl");
            AddCodeLine("rol sreg");

            ToShift--;
        }
    }
}

void g_816_neg(unsigned Flags)
/* Adds code to negate the value in the primary register. */
{
    switch (Flags & CF_TYPEMASK) {
        case CF_CHAR:
            if (Flags & CF_FORCECHAR) {
                AddCodeLine("eor #$00FF");
                AddCodeLine("inc");
                break;
            }
            /* Fall through... */
        case CF_INT:
            AddCodeLine("eor #$FFFF");
            AddCodeLine("inc");
            break;

        case CF_LONG:
            AddCodeLine("eor #$FFFF");
            AddCodeLine("clc");
            AddCodeLine("adc #$0001");
            AddCodeLine("tax"); /* Make sure to preserve the carry flag. */
            AddCodeLine("lda sreg");
            AddCodeLine("eor #$FFFF");
            AddCodeLine("adc #$0000");
            AddCodeLine("sta sreg");
            AddCodeLine("txa");
            break;

        default:
            TypeError(Flags);
            return; /* Don't proceed with bad types. */
    }
}

void g_816_bneg(unsigned Flags)
/* Adds code to perform a bitwise not on the primary register. */
{
    unsigned DoneLabel = GetLocalLabel();

    switch (Flags & CF_TYPEMASK) {
        case CF_CHAR:
            if (Flags & CF_FORCECHAR) {
                AddCodeLine("bit #$00FF");
                AddCodeLine("beq %s", LocalLabelName(DoneLabel));
                AddCodeLine("lda #$0001");
                g_defcodelabel(DoneLabel);
                break;
            }
        /* Fall through... */
        case CF_INT:
            AddCodeLine("cmp #$0000");
            AddCodeLine("beq %s", LocalLabelName(DoneLabel));
            AddCodeLine("lda #$0001");
            g_defcodelabel(DoneLabel);
            break;

        case CF_LONG:
            AddCodeLine("ora sreg");
            AddCodeLine("beq %s", LocalLabelName(DoneLabel));
            AddCodeLine("stz sreg");
            AddCodeLine("lda #$0001");
            break;

        default:
            TypeError(Flags);
            return; /* Don't proceed with bad types. */
    }
}

void g_816_com(unsigned Flags)
/* Adds code to change the primary register to its complement. */
{
    switch (Flags & CF_TYPEMASK) {
        case CF_CHAR:
            if (Flags & CF_FORCECHAR) {
                AddCodeLine("eor #$00FF");
                break;
            }
        /* Fall through... */
        case CF_INT:
            AddCodeLine("eor #$FFFF");
            break;

        case CF_LONG:
            AddCodeLine("eor #$FFFF");
            AddCodeLine("tax");
            AddCodeLine("lda sreg");
            AddCodeLine("eor #$FFFF");
            AddCodeLine("sta sreg");
            AddCodeLine("txa");
            break;

        default:
            TypeError(Flags);
            return; /* Don't proceed with bad types. */
    }
}

void g_816_inc(unsigned Flags, unsigned long Val)
/* Adds code to increment the primary register by some non-zero value. */
{
    switch (Flags & CF_TYPEMASK) {
        case CF_CHAR:
            if (Flags & CF_FORCECHAR) {
                if (Val == 1) {
                    AddCodeLine("inc");
                } else {
                    AddCodeLine("clc");
                    AddCodeLine("adc #%04X", M_BYTE(Val));
                }
                break;
            }
            /* Fall through... */
        case CF_INT:
            if (Val == 1) {
                AddCodeLine("inc");
            } else {
                AddCodeLine("clc");
                AddCodeLine("adc #$%04X", M_WORD(Val));
            }
            break;

        case CF_LONG:
            AddCodeLine("clc");
            AddCodeLine("adc #$%04X", M_WORD(Val));
            if (M_WORD(Val >> 32) != 0) {
                AddCodeLine("tax");
                AddCodeLine("lda sreg");
                AddCodeLine("adc #$%04X", M_WORD(Val >> 32));
                AddCodeLine("sta sreg");
                AddCodeLine("txa");
            } else {
                AddCodeLine("adc #$%04X", M_WORD(Val >> 32));
            }
            break;

        default:
            TypeError(Flags);
            return; /* Don't proceed with bad types. */
    }
}

void g_816_dec(unsigned Flags, unsigned long Val)
/* Adds code to decrement the primary register by some non-zero value. */
{
    switch (Flags & CF_TYPEMASK) {
        case CF_CHAR:
            if (Flags & CF_FORCECHAR) {
                if (Val == 1) {
                    AddCodeLine("dex");
                } else {
                    AddCodeLine("sec");
                    AddCodeLine("sbc #%04X", M_BYTE(Val));
                }
                break;
            }
            /* Fall through... */
        case CF_INT:
            if (Val == 1) {
                AddCodeLine("dec");
            } else {
                AddCodeLine("sec");
                AddCodeLine("sbc #$%04X", M_WORD(Val));
            }
            break;

        case CF_LONG:
            AddCodeLine("sec");
            AddCodeLine("sbc #$%04X", M_WORD(Val));
            if (M_WORD(Val >> 32) != 0) {
                AddCodeLine("tax");
                AddCodeLine("lda sreg");
                AddCodeLine("sbc #$%04X", M_WORD(Val >> 32));
                AddCodeLine("sta sreg");
                AddCodeLine("txa");
            } else {
                AddCodeLine("sbc #$%04X", M_WORD(Val >> 32));
            }
            break;

        default:
            TypeError(Flags);
            return; /* Don't proceed with bad types. */
    }
}

void g_816_eq(unsigned Flags, unsigned long Val)
/* Adds code to test for equality with the TOS and the primary register. */
{
    switch (Flags & CF_TYPEMASK) {
        case CF_CHAR:
            if (Flags & CF_FORCECHAR) {
                AddCodeLine("sep #$20");
                if (Flags & CF_CONST) {
                    AddCodeLine("cmp #$%02X", M_BYTE(Val));
                } else {
                    AddCodeLine("cmp $01,s");
                }

                AddCodeLine("rep #$20");
            }
            break;
        /* Fall through... */
        case CF_INT:
            break;

        case CF_LONG:
            break;

        default:
            TypeError(Flags);
            return; /* Don't proceed with bad types. */
    }
}

void g_816_ne(unsigned Flags, unsigned long Val)
/* Adds code to test for inequality with the TOS and the primary register. */
{
    switch (Flags & CF_TYPEMASK) {
    case CF_CHAR:
        if (Flags & CF_FORCECHAR) {

        }
        break;
        /* Fall through... */
    case CF_INT:
        break;

    case CF_LONG:
        break;

    default:
        TypeError(Flags);
        return; /* Don't proceed with bad types. */
    }
}

void g_816_lt(unsigned Flags, unsigned long Val)
/* Adds code to test if the TOS is less than the primary register. */
{
    switch (Flags & CF_TYPEMASK) {
    case CF_CHAR:
        if (Flags & CF_FORCECHAR) {

        }
        break;
        /* Fall through... */
    case CF_INT:
        break;

    case CF_LONG:
        break;

    default:
        TypeError(Flags);
        return; /* Don't proceed with bad types. */
    }
}

void g_816_le(unsigned Flags, unsigned long Val)
/* Adds code to test if the TOS is lesser or equal to the primary register. */
{
    switch (Flags & CF_TYPEMASK) {
    case CF_CHAR:
        if (Flags & CF_FORCECHAR) {

        }
        break;
        /* Fall through... */
    case CF_INT:
        break;

    case CF_LONG:
        break;

    default:
        TypeError(Flags);
        return; /* Don't proceed with bad types. */
    }
}

void g_816_gt(unsigned Flags, unsigned long Val)
/* Adds code to test if the TOS is greater than the primary register. */
{
    switch (Flags & CF_TYPEMASK) {
    case CF_CHAR:
        if (Flags & CF_FORCECHAR) {

        }
        break;
        /* Fall through... */
    case CF_INT:
        break;

    case CF_LONG:
        break;

    default:
        TypeError(Flags);
        return; /* Don't proceed with bad types. */
    }
}

void g_816_ge(unsigned Flags, unsigned long Val)
/* Adds code to test if the TOS is greater or equal to the primary register. */
{
    switch (Flags & CF_TYPEMASK) {
    case CF_CHAR:
        if (Flags & CF_FORCECHAR) {

        }
        break;
        /* Fall through... */
    case CF_INT:
        break;

    case CF_LONG:
        break;

    default:
        TypeError(Flags);
        return; /* Don't proceed with bad types. */
    }
}
