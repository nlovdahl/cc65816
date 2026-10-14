/*****************************************************************************/
/*                                                                           */
/*                               codegen816.h                                */
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

#ifndef CODEGEN_816_H
#define CODEGEN_816_H

/*****************************************************************************/
/*                   Specialized functions for 65816 code                    */
/*****************************************************************************/
/* Note: While the 65816 uses a stack offset relative to the next available
**       byte, the inputs for 65816 codegen functions expects stack offsets
**       relative to the last byte in the stack like for the 6502. This is
**       done for the sake of simplicity.
**
**       Also, it is assumed that any stack-relative offsets have been checked.
**       Do any required checking before passing these offsets.
*/

void g_816_leave(unsigned ToDrop);
/* Adds code to handle exiting a function before returning. */

void g_816_swap_regvars(int StackOffs, int RegOffs, unsigned Bytes);
/* Adds code to swap a value in memory with another in the stack. */

void g_816_save_regvars(int RegOffs, unsigned Bytes);
/* Adds code to save some value or values in memory to the stack. */

void g_816_restore_regvars(int StackOffs, int RegOffs, unsigned Bytes);
/* Adds code to save some value or values in the stack to memory. */

void g_816_getimmed(unsigned Flags, uintptr_t Val, const char* LabelBuf);
/* Adds code to load a constant into the primary register. */

void g_816_getstatic(unsigned Flags, const char* LabelBuf);
/* Adds code to load a value from a static memory cell. */

void g_816_getlocal(unsigned Flags, int StackOffs);
/* Adds code to load a value from the stack. */

void g_816_getind(unsigned Flags, unsigned Offs);
/* Adds code to load a value indirectly. */

void g_816_leasp(int StackOffs);
/* Adds code to load the address of a specified symbol. */

void g_816_leavariadic(unsigned ArgSizeOffs, int Offs);
/* Adds code to load the address of a variadic function's parameter. */

void g_816_putstatic(unsigned Flags, const char* LabelBuf);
/* Adds code to save a value to static memory. */

void g_816_putlocal(unsigned Flags, int StackOffs, long Val);
/* Adds code to save a value inside of the stack. */

void g_816_putind(unsigned Flags, unsigned Offs);
/* Adds code to indirectly save a value to memory. */

void g_816_toslong(unsigned Flags);
/* Adds code to ensure a value on TOS is a long. */

void g_816_tosint(unsigned Flags);
/* Adds code to ensure a value on TOS is an int. */

void g_816_regint(unsigned Flags);
/* Adds code to convert the current value in the primary register to an int. */

void g_816_reglong(unsigned Flags);
/* Adds code to convert the current value in the primary register to a long. */

void g_816_addstatic(unsigned Flags, const char* LabelBuf);
/* Adds code to add a static variable to the primary register. */

void g_816_addlocal(unsigned Flags, int Offs);
/* Adds code to add a value from memory to the primary register. */

void g_816_addeqstatic(unsigned Flags, const char* LabelBuf, unsigned long Val);
/* Adds code to emit += for a static variable. */

void g_816_addeqlocal(unsigned Flags, int StackOffs, unsigned long Val);
/* Adds code to emit += for a local variable. */

void g_816_addeqind(unsigned Flags, unsigned Offs, unsigned long Val);
/* Adds code to emit += for the address in the primary register. */

void g_816_subeqstatic(unsigned Flags, const char* LabelBuf, unsigned long Val);
/* Adds code to emit -= for a static variable. */

void g_816_subeqlocal(unsigned Flags, int StackOffs, unsigned long Val);
/* Adds code to emit -= for a local variable. */

void g_816_subeqind(unsigned Flags, unsigned Offs, unsigned long Val);
/* Adds code to emit -= for the address in the primary register. */

void g_816_addaddr_local(int StackOffs);
/* Adds code to add a local variable's address to the primary register. */

void g_816_addaddr_static(const char* LabelBuf);
/* Adds code to add a static variable's address to the primary register. */

void g_816_save(unsigned Flags);
/* Adds code to save the primary register to the hold register. */

void g_816_restore(unsigned Flags);
/* Adds code to restore the hold register to the primary register. */

void g_816_cmp(unsigned Flags, unsigned long Val);
/* Adds code to perform an immediate comparison (primary reg unchanged). */

void g_816_test(unsigned Flags);
/* Adds code to test the value in the primary register. */

void g_816_push(unsigned Flags, unsigned long Val);
/* Adds code to push the primary register's value or a constant to the stack. */

void g_816_swap(unsigned Flags);
/* Adds code to swap the top of the stack and the primary register. */

void g_816_callind(unsigned Flags, unsigned ArgSize, int StackOffs);
/* Adds code to call a subroutine indirectly. */

void g_816_lateadjustsp(unsigned Label);
/* Adds code to adjust the stack pointer pased on non-immediate data. */

void g_816_drop(unsigned Space);
/* Adds code to drop space on the stack. */

void g_816_space(unsigned Space);
/* Adds code to create space on the stack. */

void g_816_add(unsigned Flags, unsigned long Val);
/* Adds code to add the TOS to the primary register. */

void g_816_sub(unsigned Flags, unsigned long Val);
/* Adds code to subtract the primary register from the TOS. */

void g_816_rsub(unsigned Flags, unsigned long Val);
/* Adds code to subtract the TOS to the primary register. */

void g_816_mul(unsigned Flags, unsigned long Val);
/* Adds code to multiply the primary register with the TOS. */

void g_816_div(unsigned Flags, unsigned long Val);
/* Adds code to divide the TOS by the primary register. */

void g_816_mod(unsigned Flags, unsigned long Val);
/* Adds code to modulate the TOS by the primary register. */

void g_816_or(unsigned Flags, unsigned long Val);
/* Adds code to perform a bitwise or with the TOS and the primary register. */

void g_816_xor(unsigned Flags, unsigned long Val);
/* Adds code to perform a bitwise xor with the TOS and the primary register. */

void g_816_and(unsigned Flags, unsigned long Val);
/* Adds code to perform a bitwise and with the TOS and the primary register. */

void g_816_lsr(unsigned Flags, unsigned long Val);
/* Adds code to log. shift TOS right by the value in the primary register. */

void g_816_asr(unsigned Flags, unsigned long Val);
/* Adds code to arith. shift TOS right by the value in the primary register. */

void g_816_asl(unsigned Flags, unsigned long Val);
/* Adds code to arith. shift TOS left by the value in the primary register. */

void g_816_neg(unsigned Flags);
/* Adds code to negate the value in the primary register. */

void g_816_bneg(unsigned Flags);
/* Adds code to perform a bitwise not on the primary register. */

void g_816_com(unsigned Flags);
/* Adds code to change the primary register to its complement. */

void g_816_inc(unsigned Flags, unsigned long Val);
/* Adds code to increment the primary register by some non-zero value. */

void g_816_dec(unsigned Flags, unsigned long Val);
/* Adds code to decrement the primary register by some non-zero value. */

/* Note: tests should change the primary register to 1 if true, 0 otherwise. */

void g_816_eq(unsigned Flags, unsigned long Val);
/* Adds code to test for equality with the TOS and the primary register. */

void g_816_ne(unsigned Flags, unsigned long Val);
/* Adds code to test for inequality with the TOS and the primary register. */

void g_816_lt(unsigned Flags, unsigned long Val);
/* Adds code to test if the TOS is less than the primary register. */

void g_816_le(unsigned Flags, unsigned long Val);
/* Adds code to test if the TOS is lesser or equal to the primary register. */

void g_816_gt(unsigned Flags, unsigned long Val);
/* Adds code to test if the TOS is greater than the primary register. */

void g_816_ge(unsigned Flags, unsigned long Val);
/* Adds code to test if the TOS is greater or equal to the primary register. */

void g_816_initregister(unsigned Label, unsigned Reg, unsigned Size);
/* Adds code to initialize a register variable from static data. */

void g_816_initauto(unsigned Label, unsigned Size);
/* Adds code to initialize a local variable from static data. */

void g_816_initstatic(unsigned InitLabel, unsigned VarLabel, unsigned Size);
/* Adds code to initialize a static local variable from static data. */

void g_816_testbitfield(unsigned BitOffs, unsigned BitWidth);
/* Adds code to test a bit-field in the primary register. */

void g_816_extractbitfield(unsigned FullWidthFlags,
                           unsigned long ZeroExtendMask, unsigned SignBitPos,
                           unsigned SignBitByte, unsigned SignBitPosInByte);
/* Adds code to extract a signed bit-field. */

const char* g_816_switchcmp(unsigned Depth);
/* Returns the compare instruction used to implement a switch statement,
** adding code where appropriate.
*/

#endif  /* CODEGEN_816_H */
