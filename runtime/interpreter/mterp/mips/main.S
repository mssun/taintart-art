%def header():
/*
 * Copyright (C) 2016 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/*
  Art assembly interpreter notes:

  First validate assembly code by implementing ExecuteXXXImpl() style body (doesn't
  handle invoke, allows higher-level code to create frame & shadow frame.

  Once that's working, support direct entry code & eliminate shadow frame (and
  excess locals allocation.

  Some (hopefully) temporary ugliness.  We'll treat rFP as pointing to the
  base of the vreg array within the shadow frame.  Access the other fields,
  dex_pc_, method_ and number_of_vregs_ via negative offsets.  For now, we'll continue
  the shadow frame mechanism of double-storing object references - via rFP &
  number_of_vregs_.

 */

#include "asm_support.h"
#include "interpreter/cfi_asm_support.h"

#if (__mips==32) && (__mips_isa_rev>=2)
#define MIPS32REVGE2    /* mips32r2 and greater */
#if (__mips==32) && (__mips_isa_rev>=5)
#define FPU64           /* 64 bit FPU */
#if (__mips==32) && (__mips_isa_rev>=6)
#define MIPS32REVGE6    /* mips32r6 and greater */
#endif
#endif
#endif

/* MIPS definitions and declarations

   reg  nick      purpose
   s0   rPC       interpreted program counter, used for fetching instructions
   s1   rFP       interpreted frame pointer, used for accessing locals and args
   s2   rSELF     self (Thread) pointer
   s3   rIBASE    interpreted instruction base pointer, used for computed goto
   s4   rINST     first 16-bit code unit of current instruction
   s5   rOBJ      object pointer
   s6   rREFS     base of object references in shadow frame (ideally, we'll get rid of this later).
   s7   rTEMP     used as temp storage that can survive a function call
   s8   rPROFILE  branch profiling countdown

*/

/* single-purpose registers, given names for clarity */
#define rPC s0
#define CFI_DEX 16  // DWARF register number of the register holding dex-pc (s0).
#define CFI_TMP 4   // DWARF register number of the first argument register (a0).
#define rFP s1
#define rSELF s2
#define rIBASE s3
#define rINST s4
#define rOBJ s5
#define rREFS s6
#define rTEMP s7
#define rPROFILE s8

#define rARG0 a0
#define rARG1 a1
#define rARG2 a2
#define rARG3 a3
#define rRESULT0 v0
#define rRESULT1 v1

/* GP register definitions */
#define zero    $$0      /* always zero */
#define AT      $$at     /* assembler temp */
#define v0      $$2      /* return value */
#define v1      $$3
#define a0      $$4      /* argument registers */
#define a1      $$5
#define a2      $$6
#define a3      $$7
#define t0      $$8      /* temp registers (not saved across subroutine calls) */
#define t1      $$9
#define t2      $$10
#define t3      $$11
#define t4      $$12
#define t5      $$13
#define t6      $$14
#define t7      $$15
#define ta0     $$12     /* alias */
#define ta1     $$13
#define ta2     $$14
#define ta3     $$15
#define s0      $$16     /* saved across subroutine calls (callee saved) */
#define s1      $$17
#define s2      $$18
#define s3      $$19
#define s4      $$20
#define s5      $$21
#define s6      $$22
#define s7      $$23
#define t8      $$24     /* two more temp registers */
#define t9      $$25
#define k0      $$26     /* kernel temporary */
#define k1      $$27
#define gp      $$28     /* global pointer */
#define sp      $$29     /* stack pointer */
#define s8      $$30     /* one more callee saved */
#define ra      $$31     /* return address */

/* FP register definitions */
#define fv0    $$f0
#define fv0f   $$f1
#define fv1    $$f2
#define fv1f   $$f3
#define fa0    $$f12
#define fa0f   $$f13
#define fa1    $$f14
#define fa1f   $$f15
#define ft0    $$f4
#define ft0f   $$f5
#define ft1    $$f6
#define ft1f   $$f7
#define ft2    $$f8
#define ft2f   $$f9
#define ft3    $$f10
#define ft3f   $$f11
#define ft4    $$f16
#define ft4f   $$f17
#define ft5    $$f18
#define ft5f   $$f19
#define fs0    $$f20
#define fs0f   $$f21
#define fs1    $$f22
#define fs1f   $$f23
#define fs2    $$f24
#define fs2f   $$f25
#define fs3    $$f26
#define fs3f   $$f27
#define fs4    $$f28
#define fs4f   $$f29
#define fs5    $$f30
#define fs5f   $$f31

#ifndef MIPS32REVGE6
#define fcc0   $$fcc0
#define fcc1   $$fcc1
#endif

#ifdef MIPS32REVGE2
#define SEB(rd, rt) \
    seb       rd, rt
#define SEH(rd, rt) \
    seh       rd, rt
#define INSERT_HIGH_HALF(rd_lo, rt_hi) \
    ins       rd_lo, rt_hi, 16, 16
#else
#define SEB(rd, rt) \
    sll       rd, rt, 24; \
    sra       rd, rd, 24
#define SEH(rd, rt) \
    sll       rd, rt, 16; \
    sra       rd, rd, 16
/* Clobbers rt_hi on pre-R2. */
#define INSERT_HIGH_HALF(rd_lo, rt_hi) \
    sll       rt_hi, rt_hi, 16; \
    or        rd_lo, rt_hi
#endif

#ifdef FPU64
#define MOVE_TO_FPU_HIGH(r, flo, fhi) \
    mthc1     r, flo
#else
#define MOVE_TO_FPU_HIGH(r, flo, fhi) \
    mtc1      r, fhi
#endif

#ifdef MIPS32REVGE6
#define JR(rt) \
    jic       rt, 0
#define LSA(rd, rs, rt, sa) \
    .if sa; \
    lsa       rd, rs, rt, sa; \
    .else; \
    addu      rd, rs, rt; \
    .endif
#else
#define JR(rt) \
    jalr      zero, rt
#define LSA(rd, rs, rt, sa) \
    .if sa; \
    .set      push; \
    .set      noat; \
    sll       AT, rs, sa; \
    addu      rd, AT, rt; \
    .set      pop; \
    .else; \
    addu      rd, rs, rt; \
    .endif
#endif

/*
 * Instead of holding a pointer to the shadow frame, we keep rFP at the base of the vregs.  So,
 * to access other shadow frame fields, we need to use a backwards offset.  Define those here.
 */
#define OFF_FP(a) (a - SHADOWFRAME_VREGS_OFFSET)
#define OFF_FP_NUMBER_OF_VREGS OFF_FP(SHADOWFRAME_NUMBER_OF_VREGS_OFFSET)
#define OFF_FP_DEX_PC OFF_FP(SHADOWFRAME_DEX_PC_OFFSET)
#define OFF_FP_LINK OFF_FP(SHADOWFRAME_LINK_OFFSET)
#define OFF_FP_METHOD OFF_FP(SHADOWFRAME_METHOD_OFFSET)
#define OFF_FP_RESULT_REGISTER OFF_FP(SHADOWFRAME_RESULT_REGISTER_OFFSET)
#define OFF_FP_DEX_PC_PTR OFF_FP(SHADOWFRAME_DEX_PC_PTR_OFFSET)
#define OFF_FP_DEX_INSTRUCTIONS OFF_FP(SHADOWFRAME_DEX_INSTRUCTIONS_OFFSET)
#define OFF_FP_SHADOWFRAME OFF_FP(0)

#define MTERP_PROFILE_BRANCHES 1
#define MTERP_LOGGING 0

/*
 * "export" the PC to dex_pc field in the shadow frame, f/b/o future exception objects.  Must
 * be done *before* something throws.
 *
 * It's okay to do this more than once.
 *
 * NOTE: the fast interpreter keeps track of dex pc as a direct pointer to the mapped
 * dex byte codes.  However, the rest of the runtime expects dex pc to be an instruction
 * offset into the code_items_[] array.  For effiency, we will "export" the
 * current dex pc as a direct pointer using the EXPORT_PC macro, and rely on GetDexPC
 * to convert to a dex pc when needed.
 */
#define EXPORT_PC() \
    sw        rPC, OFF_FP_DEX_PC_PTR(rFP)

#define EXPORT_DEX_PC(tmp) \
    lw        tmp, OFF_FP_DEX_INSTRUCTIONS(rFP); \
    sw        rPC, OFF_FP_DEX_PC_PTR(rFP); \
    subu      tmp, rPC, tmp; \
    sra       tmp, tmp, 1; \
    sw        tmp, OFF_FP_DEX_PC(rFP)

/*
 * Fetch the next instruction from rPC into rINST.  Does not advance rPC.
 */
#define FETCH_INST() lhu rINST, (rPC)

/*
 * Fetch the next instruction from the specified offset.  Advances rPC
 * to point to the next instruction.  "_count" is in 16-bit code units.
 *
 * This must come AFTER anything that can throw an exception, or the
 * exception catch may miss.  (This also implies that it must come after
 * EXPORT_PC().)
 */
#define FETCH_ADVANCE_INST(_count) \
    lhu       rINST, ((_count)*2)(rPC); \
    addu      rPC, rPC, ((_count) * 2)

/*
 * Similar to FETCH_ADVANCE_INST, but does not update rPC.  Used to load
 * rINST ahead of possible exception point.  Be sure to manually advance rPC
 * later.
 */
#define PREFETCH_INST(_count) lhu rINST, ((_count)*2)(rPC)

/* Advance rPC by some number of code units. */
#define ADVANCE(_count) addu rPC, rPC, ((_count) * 2)

/*
 * Fetch the next instruction from an offset specified by rd.  Updates
 * rPC to point to the next instruction.  "rd" must specify the distance
 * in bytes, *not* 16-bit code units, and may be a signed value.
 */
#define FETCH_ADVANCE_INST_RB(rd) \
    addu      rPC, rPC, rd; \
    lhu       rINST, (rPC)

/*
 * Fetch a half-word code unit from an offset past the current PC.  The
 * "_count" value is in 16-bit code units.  Does not advance rPC.
 *
 * The "_S" variant works the same but treats the value as signed.
 */
#define FETCH(rd, _count) lhu rd, ((_count) * 2)(rPC)
#define FETCH_S(rd, _count) lh rd, ((_count) * 2)(rPC)

/*
 * Fetch one byte from an offset past the current PC.  Pass in the same
 * "_count" as you would for FETCH, and an additional 0/1 indicating which
 * byte of the halfword you want (lo/hi).
 */
#define FETCH_B(rd, _count, _byte) lbu rd, ((_count) * 2 + _byte)(rPC)

/*
 * Put the instruction's opcode field into the specified register.
 */
#define GET_INST_OPCODE(rd) and rd, rINST, 0xFF

/*
 * Transform opcode into branch target address.
 */
#define GET_OPCODE_TARGET(rd) \
    sll       rd, rd, ${handler_size_bits}; \
    addu      rd, rIBASE, rd

/*
 * Begin executing the opcode in rd.
 */
#define GOTO_OPCODE(rd) \
    GET_OPCODE_TARGET(rd); \
    JR(rd)

/*
 * Get/set the 32-bit value from a Dalvik register.
 */
#define GET_VREG(rd, rix) LOAD_eas2(rd, rFP, rix)

#define GET_VREG_F(rd, rix) \
    .set noat; \
    EAS2(AT, rFP, rix); \
    l.s       rd, (AT); \
    .set at

#ifdef MIPS32REVGE6
#define SET_VREG(rd, rix) \
    lsa       t8, rix, rFP, 2; \
    sw        rd, 0(t8); \
    lsa       t8, rix, rREFS, 2; \
    sw        zero, 0(t8)
#else
#define SET_VREG(rd, rix) \
    .set noat; \
    sll       AT, rix, 2; \
    addu      t8, rFP, AT; \
    sw        rd, 0(t8); \
    addu      t8, rREFS, AT; \
    .set at; \
    sw        zero, 0(t8)
#endif

#ifdef MIPS32REVGE6
#define SET_VREG_OBJECT(rd, rix) \
    lsa       t8, rix, rFP, 2; \
    sw        rd, 0(t8); \
    lsa       t8, rix, rREFS, 2; \
    sw        rd, 0(t8)
#else
#define SET_VREG_OBJECT(rd, rix) \
    .set noat; \
    sll       AT, rix, 2; \
    addu      t8, rFP, AT; \
    sw        rd, 0(t8); \
    addu      t8, rREFS, AT; \
    .set at; \
    sw        rd, 0(t8)
#endif

#ifdef MIPS32REVGE6
#define SET_VREG64(rlo, rhi, rix) \
    lsa       t8, rix, rFP, 2; \
    sw        rlo, 0(t8); \
    sw        rhi, 4(t8); \
    lsa       t8, rix, rREFS, 2; \
    sw        zero, 0(t8); \
    sw        zero, 4(t8)
#else
#define SET_VREG64(rlo, rhi, rix) \
    .set noat; \
    sll       AT, rix, 2; \
    addu      t8, rFP, AT; \
    sw        rlo, 0(t8); \
    sw        rhi, 4(t8); \
    addu      t8, rREFS, AT; \
    .set at; \
    sw        zero, 0(t8); \
    sw        zero, 4(t8)
#endif

#ifdef MIPS32REVGE6
#define SET_VREG_F(rd, rix) \
    lsa       t8, rix, rFP, 2; \
    s.s       rd, 0(t8); \
    lsa       t8, rix, rREFS, 2; \
    sw        zero, 0(t8)
#else
#define SET_VREG_F(rd, rix) \
    .set noat; \
    sll       AT, rix, 2; \
    addu      t8, rFP, AT; \
    s.s       rd, 0(t8); \
    addu      t8, rREFS, AT; \
    .set at; \
    sw        zero, 0(t8)
#endif

#ifdef MIPS32REVGE6
#define SET_VREG64_F(rlo, rhi, rix) \
    lsa       t8, rix, rFP, 2; \
    .set noat; \
    mfhc1     AT, rlo; \
    s.s       rlo, 0(t8); \
    sw        AT, 4(t8); \
    .set at; \
    lsa       t8, rix, rREFS, 2; \
    sw        zero, 0(t8); \
    sw        zero, 4(t8)
#elif defined(FPU64)
#define SET_VREG64_F(rlo, rhi, rix) \
    .set noat; \
    sll       AT, rix, 2; \
    addu      t8, rREFS, AT; \
    sw        zero, 0(t8); \
    sw        zero, 4(t8); \
    addu      t8, rFP, AT; \
    mfhc1     AT, rlo; \
    sw        AT, 4(t8); \
    .set at; \
    s.s       rlo, 0(t8)
#else
#define SET_VREG64_F(rlo, rhi, rix) \
    .set noat; \
    sll       AT, rix, 2; \
    addu      t8, rFP, AT; \
    s.s       rlo, 0(t8); \
    s.s       rhi, 4(t8); \
    addu      t8, rREFS, AT; \
    .set at; \
    sw        zero, 0(t8); \
    sw        zero, 4(t8)
#endif

/* Combination of the SET_VREG and GOTO_OPCODE functions to save 1 instruction */
#ifdef MIPS32REVGE6
#define SET_VREG_GOTO(rd, rix, dst) \
    .set noreorder; \
    GET_OPCODE_TARGET(dst); \
    lsa       t8, rix, rFP, 2; \
    sw        rd, 0(t8); \
    lsa       t8, rix, rREFS, 2; \
    jalr      zero, dst; \
    sw        zero, 0(t8); \
    .set reorder
#else
#define SET_VREG_GOTO(rd, rix, dst) \
    .set noreorder; \
    GET_OPCODE_TARGET(dst); \
    .set noat; \
    sll       AT, rix, 2; \
    addu      t8, rFP, AT; \
    sw        rd, 0(t8); \
    addu      t8, rREFS, AT; \
    .set at; \
    jalr      zero, dst; \
    sw        zero, 0(t8); \
    .set reorder
#endif

/* Combination of the SET_VREG_OBJECT and GOTO_OPCODE functions to save 1 instruction */
#ifdef MIPS32REVGE6
#define SET_VREG_OBJECT_GOTO(rd, rix, dst) \
    .set noreorder; \
    GET_OPCODE_TARGET(dst); \
    lsa       t8, rix, rFP, 2; \
    sw        rd, 0(t8); \
    lsa       t8, rix, rREFS, 2; \
    jalr      zero, dst; \
    sw        rd, 0(t8); \
    .set reorder
#else
#define SET_VREG_OBJECT_GOTO(rd, rix, dst) \
    .set noreorder; \
    GET_OPCODE_TARGET(dst); \
    .set noat; \
    sll       AT, rix, 2; \
    addu      t8, rFP, AT; \
    sw        rd, 0(t8); \
    addu      t8, rREFS, AT; \
    .set at; \
    jalr      zero, dst; \
    sw        rd, 0(t8); \
    .set reorder
#endif

/* Combination of the SET_VREG64 and GOTO_OPCODE functions to save 1 instruction */
#ifdef MIPS32REVGE6
#define SET_VREG64_GOTO(rlo, rhi, rix, dst) \
    .set noreorder; \
    GET_OPCODE_TARGET(dst); \
    lsa       t8, rix, rFP, 2; \
    sw        rlo, 0(t8); \
    sw        rhi, 4(t8); \
    lsa       t8, rix, rREFS, 2; \
    sw        zero, 0(t8); \
    jalr      zero, dst; \
    sw        zero, 4(t8); \
    .set reorder
#else
#define SET_VREG64_GOTO(rlo, rhi, rix, dst) \
    .set noreorder; \
    GET_OPCODE_TARGET(dst); \
    .set noat; \
    sll       AT, rix, 2; \
    addu      t8, rFP, AT; \
    sw        rlo, 0(t8); \
    sw        rhi, 4(t8); \
    addu      t8, rREFS, AT; \
    .set at; \
    sw        zero, 0(t8); \
    jalr      zero, dst; \
    sw        zero, 4(t8); \
    .set reorder
#endif

/* Combination of the SET_VREG_F and GOTO_OPCODE functions to save 1 instruction */
#ifdef MIPS32REVGE6
#define SET_VREG_F_GOTO(rd, rix, dst) \
    .set noreorder; \
    GET_OPCODE_TARGET(dst); \
    lsa       t8, rix, rFP, 2; \
    s.s       rd, 0(t8); \
    lsa       t8, rix, rREFS, 2; \
    jalr      zero, dst; \
    sw        zero, 0(t8); \
    .set reorder
#else
#define SET_VREG_F_GOTO(rd, rix, dst) \
    .set noreorder; \
    GET_OPCODE_TARGET(dst); \
    .set noat; \
    sll       AT, rix, 2; \
    addu      t8, rFP, AT; \
    s.s       rd, 0(t8); \
    addu      t8, rREFS, AT; \
    .set at; \
    jalr      zero, dst; \
    sw        zero, 0(t8); \
    .set reorder
#endif

/* Combination of the SET_VREG64_F and GOTO_OPCODE functions to save 1 instruction */
#ifdef MIPS32REVGE6
#define SET_VREG64_F_GOTO(rlo, rhi, rix, dst) \
    .set noreorder; \
    GET_OPCODE_TARGET(dst); \
    lsa       t8, rix, rFP, 2; \
    .set noat; \
    mfhc1     AT, rlo; \
    s.s       rlo, 0(t8); \
    sw        AT, 4(t8); \
    .set at; \
    lsa       t8, rix, rREFS, 2; \
    sw        zero, 0(t8); \
    jalr      zero, dst; \
    sw        zero, 4(t8); \
    .set reorder
#elif defined(FPU64)
#define SET_VREG64_F_GOTO(rlo, rhi, rix, dst) \
    .set noreorder; \
    GET_OPCODE_TARGET(dst); \
    .set noat; \
    sll       AT, rix, 2; \
    addu      t8, rREFS, AT; \
    sw        zero, 0(t8); \
    sw        zero, 4(t8); \
    addu      t8, rFP, AT; \
    mfhc1     AT, rlo; \
    sw        AT, 4(t8); \
    .set at; \
    jalr      zero, dst; \
    s.s       rlo, 0(t8); \
    .set reorder
#else
#define SET_VREG64_F_GOTO(rlo, rhi, rix, dst) \
    .set noreorder; \
    GET_OPCODE_TARGET(dst); \
    .set noat; \
    sll       AT, rix, 2; \
    addu      t8, rFP, AT; \
    s.s       rlo, 0(t8); \
    s.s       rhi, 4(t8); \
    addu      t8, rREFS, AT; \
    .set at; \
    sw        zero, 0(t8); \
    jalr      zero, dst; \
    sw        zero, 4(t8); \
    .set reorder
#endif

#define GET_OPA(rd) srl rd, rINST, 8
#ifdef MIPS32REVGE2
#define GET_OPA4(rd) ext rd, rINST, 8, 4
#else
#define GET_OPA4(rd) GET_OPA(rd); and rd, 0xf
#endif
#define GET_OPB(rd) srl rd, rINST, 12

/*
 * Form an Effective Address rd = rbase + roff<<shift;
 * Uses reg AT on pre-R6.
 */
#define EASN(rd, rbase, roff, shift) LSA(rd, roff, rbase, shift)

#define EAS1(rd, rbase, roff) EASN(rd, rbase, roff, 1)
#define EAS2(rd, rbase, roff) EASN(rd, rbase, roff, 2)
#define EAS3(rd, rbase, roff) EASN(rd, rbase, roff, 3)
#define EAS4(rd, rbase, roff) EASN(rd, rbase, roff, 4)

#define LOAD_eas2(rd, rbase, roff) \
    .set noat; \
    EAS2(AT, rbase, roff); \
    lw        rd, 0(AT); \
    .set at

#define STORE_eas2(rd, rbase, roff) \
    .set noat; \
    EAS2(AT, rbase, roff); \
    sw        rd, 0(AT); \
    .set at

#define LOAD_RB_OFF(rd, rbase, off) lw rd, off(rbase)
#define STORE_RB_OFF(rd, rbase, off) sw rd, off(rbase)

#define STORE64_off(rlo, rhi, rbase, off) \
    sw        rlo, off(rbase); \
    sw        rhi, (off+4)(rbase)
#define LOAD64_off(rlo, rhi, rbase, off) \
    lw        rlo, off(rbase); \
    lw        rhi, (off+4)(rbase)

#define STORE64(rlo, rhi, rbase) STORE64_off(rlo, rhi, rbase, 0)
#define LOAD64(rlo, rhi, rbase) LOAD64_off(rlo, rhi, rbase, 0)

#ifdef FPU64
#define STORE64_off_F(rlo, rhi, rbase, off) \
    s.s       rlo, off(rbase); \
    .set noat; \
    mfhc1     AT, rlo; \
    sw        AT, (off+4)(rbase); \
    .set at
#define LOAD64_off_F(rlo, rhi, rbase, off) \
    l.s       rlo, off(rbase); \
    .set noat; \
    lw        AT, (off+4)(rbase); \
    mthc1     AT, rlo; \
    .set at
#else
#define STORE64_off_F(rlo, rhi, rbase, off) \
    s.s       rlo, off(rbase); \
    s.s       rhi, (off+4)(rbase)
#define LOAD64_off_F(rlo, rhi, rbase, off) \
    l.s       rlo, off(rbase); \
    l.s       rhi, (off+4)(rbase)
#endif

#define STORE64_F(rlo, rhi, rbase) STORE64_off_F(rlo, rhi, rbase, 0)
#define LOAD64_F(rlo, rhi, rbase) LOAD64_off_F(rlo, rhi, rbase, 0)

#define LOAD_base_offMirrorArray_length(rd, rbase) LOAD_RB_OFF(rd, rbase, MIRROR_ARRAY_LENGTH_OFFSET)

#define STACK_STORE(rd, off) sw rd, off(sp)
#define STACK_LOAD(rd, off) lw rd, off(sp)
#define CREATE_STACK(n) subu sp, sp, n
#define DELETE_STACK(n) addu sp, sp, n

#define LOAD_ADDR(dest, addr) la dest, addr
#define LOAD_IMM(dest, imm) li dest, imm
#define MOVE_REG(dest, src) move dest, src
#define STACK_SIZE 128

#define STACK_OFFSET_ARG04 16
#define STACK_OFFSET_ARG05 20
#define STACK_OFFSET_ARG06 24
#define STACK_OFFSET_ARG07 28
#define STACK_OFFSET_GP    84

#define JAL(n) jal n
#define BAL(n) bal n

/*
 * FP register usage restrictions:
 * 1) We don't use the callee save FP registers so we don't have to save them.
 * 2) We don't use the odd FP registers so we can share code with mips32r6.
 */
#define STACK_STORE_FULL() CREATE_STACK(STACK_SIZE); \
    STACK_STORE(ra, 124); \
    STACK_STORE(s8, 120); \
    STACK_STORE(s0, 116); \
    STACK_STORE(s1, 112); \
    STACK_STORE(s2, 108); \
    STACK_STORE(s3, 104); \
    STACK_STORE(s4, 100); \
    STACK_STORE(s5, 96); \
    STACK_STORE(s6, 92); \
    STACK_STORE(s7, 88);

#define STACK_LOAD_FULL() STACK_LOAD(gp, STACK_OFFSET_GP); \
    STACK_LOAD(s7, 88); \
    STACK_LOAD(s6, 92); \
    STACK_LOAD(s5, 96); \
    STACK_LOAD(s4, 100); \
    STACK_LOAD(s3, 104); \
    STACK_LOAD(s2, 108); \
    STACK_LOAD(s1, 112); \
    STACK_LOAD(s0, 116); \
    STACK_LOAD(s8, 120); \
    STACK_LOAD(ra, 124); \
    DELETE_STACK(STACK_SIZE)

#define REFRESH_IBASE() \
    lw        rIBASE, THREAD_CURRENT_IBASE_OFFSET(rSELF)

/* Constants for float/double_to_int/long conversions */
#define INT_MIN                 0x80000000
#define INT_MIN_AS_FLOAT        0xCF000000
#define INT_MIN_AS_DOUBLE_HIGH  0xC1E00000
#define LONG_MIN_HIGH           0x80000000
#define LONG_MIN_AS_FLOAT       0xDF000000
#define LONG_MIN_AS_DOUBLE_HIGH 0xC3E00000

%def entry():
/*
 * Copyright (C) 2016 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
/*
 * Interpreter entry point.
 */

    .text
    .align 2
    .global ExecuteMterpImpl
    .ent    ExecuteMterpImpl
    .frame sp, STACK_SIZE, ra
/*
 * On entry:
 *  a0  Thread* self
 *  a1  dex_instructions
 *  a2  ShadowFrame
 *  a3  JValue* result_register
 *
 */

ExecuteMterpImpl:
    .cfi_startproc
    .set noreorder
    .cpload t9
    .set reorder
/* Save to the stack. Frame size = STACK_SIZE */
    STACK_STORE_FULL()
/* This directive will make sure all subsequent jal restore gp at a known offset */
    .cprestore STACK_OFFSET_GP

    /* Remember the return register */
    sw      a3, SHADOWFRAME_RESULT_REGISTER_OFFSET(a2)

    /* Remember the dex instruction pointer */
    sw      a1, SHADOWFRAME_DEX_INSTRUCTIONS_OFFSET(a2)

    /* set up "named" registers */
    move    rSELF, a0
    lw      a0, SHADOWFRAME_NUMBER_OF_VREGS_OFFSET(a2)
    addu    rFP, a2, SHADOWFRAME_VREGS_OFFSET     # point to vregs.
    EAS2(rREFS, rFP, a0)                          # point to reference array in shadow frame
    lw      a0, SHADOWFRAME_DEX_PC_OFFSET(a2)     # Get starting dex_pc
    EAS1(rPC, a1, a0)                             # Create direct pointer to 1st dex opcode
    CFI_DEFINE_DEX_PC_WITH_OFFSET(CFI_TMP, CFI_DEX, 0)

    EXPORT_PC()

    /* Starting ibase */
    lw      rIBASE, THREAD_CURRENT_IBASE_OFFSET(rSELF)

    /* Set up for backwards branches & osr profiling */
    lw      a0, OFF_FP_METHOD(rFP)
    addu    a1, rFP, OFF_FP_SHADOWFRAME
    move    a2, rSELF
    JAL(MterpSetUpHotnessCountdown)        # (method, shadow_frame, self)
    move    rPROFILE, v0                   # Starting hotness countdown to rPROFILE

    /* start executing the instruction at rPC */
    FETCH_INST()                           # load rINST from rPC
    GET_INST_OPCODE(t0)                    # extract opcode from rINST
    GOTO_OPCODE(t0)                        # jump to next instruction
    /* NOTE: no fallthrough */

%def alt_stub():
/*
 * Inter-instruction transfer stub.  Call out to MterpCheckBefore to handle
 * any interesting requests and then jump to the real instruction
 * handler.    Note that the call to MterpCheckBefore is done as a tail call.
 */
    .extern MterpCheckBefore
    la     ra, artMterpAsmInstructionStart + (${opnum} * 128)   # Addr of primary handler
    lw     rIBASE, THREAD_CURRENT_IBASE_OFFSET(rSELF)           # refresh IBASE
    move   a0, rSELF                    # arg0
    addu   a1, rFP, OFF_FP_SHADOWFRAME  # arg1
    move   a2, rPC
    la     t9, MterpCheckBefore
    jalr   zero, t9                     # Tail call to Mterp(self, shadow_frame, dex_pc_ptr)

%def fallback():
/* Transfer stub to alternate interpreter */
    b    MterpFallback

%def helpers():
%  op_float_to_long_helper_code()
%  op_double_to_long_helper_code()
%  op_mul_long_helper_code()
%  op_shl_long_helper_code()
%  op_shr_long_helper_code()
%  op_ushr_long_helper_code()
%  op_shl_long_2addr_helper_code()
%  op_shr_long_2addr_helper_code()
%  op_ushr_long_2addr_helper_code()

%def footer():
/*
 * ===========================================================================
 *  Common subroutines and data
 * ===========================================================================
 */

    .text
    .align 2

/*
 * We've detected a condition that will result in an exception, but the exception
 * has not yet been thrown.  Just bail out to the reference interpreter to deal with it.
 * TUNING: for consistency, we may want to just go ahead and handle these here.
 */
common_errDivideByZero:
    EXPORT_PC()
#if MTERP_LOGGING
    move  a0, rSELF
    addu  a1, rFP, OFF_FP_SHADOWFRAME
    JAL(MterpLogDivideByZeroException)
#endif
    b MterpCommonFallback

common_errArrayIndex:
    EXPORT_PC()
#if MTERP_LOGGING
    move  a0, rSELF
    addu  a1, rFP, OFF_FP_SHADOWFRAME
    JAL(MterpLogArrayIndexException)
#endif
    b MterpCommonFallback

common_errNegativeArraySize:
    EXPORT_PC()
#if MTERP_LOGGING
    move  a0, rSELF
    addu  a1, rFP, OFF_FP_SHADOWFRAME
    JAL(MterpLogNegativeArraySizeException)
#endif
    b MterpCommonFallback

common_errNoSuchMethod:
    EXPORT_PC()
#if MTERP_LOGGING
    move  a0, rSELF
    addu  a1, rFP, OFF_FP_SHADOWFRAME
    JAL(MterpLogNoSuchMethodException)
#endif
    b MterpCommonFallback

common_errNullObject:
    EXPORT_PC()
#if MTERP_LOGGING
    move  a0, rSELF
    addu  a1, rFP, OFF_FP_SHADOWFRAME
    JAL(MterpLogNullObjectException)
#endif
    b MterpCommonFallback

common_exceptionThrown:
    EXPORT_PC()
#if MTERP_LOGGING
    move  a0, rSELF
    addu  a1, rFP, OFF_FP_SHADOWFRAME
    JAL(MterpLogExceptionThrownException)
#endif
    b MterpCommonFallback

MterpSuspendFallback:
    EXPORT_PC()
#if MTERP_LOGGING
    move  a0, rSELF
    addu  a1, rFP, OFF_FP_SHADOWFRAME
    lw    a2, THREAD_FLAGS_OFFSET(rSELF)
    JAL(MterpLogSuspendFallback)
#endif
    b MterpCommonFallback

/*
 * If we're here, something is out of the ordinary.  If there is a pending
 * exception, handle it.  Otherwise, roll back and retry with the reference
 * interpreter.
 */
MterpPossibleException:
    lw      a0, THREAD_EXCEPTION_OFFSET(rSELF)
    beqz    a0, MterpFallback          # If exception, fall back to reference interpreter.
    /* intentional fallthrough - handle pending exception. */
/*
 * On return from a runtime helper routine, we've found a pending exception.
 * Can we handle it here - or need to bail out to caller?
 *
 */
MterpException:
    move    a0, rSELF
    addu    a1, rFP, OFF_FP_SHADOWFRAME
    JAL(MterpHandleException)                    # (self, shadow_frame)
    beqz    v0, MterpExceptionReturn             # no local catch, back to caller.
    lw      a0, OFF_FP_DEX_INSTRUCTIONS(rFP)
    lw      a1, OFF_FP_DEX_PC(rFP)
    lw      rIBASE, THREAD_CURRENT_IBASE_OFFSET(rSELF)
    EAS1(rPC, a0, a1)                            # generate new dex_pc_ptr
    /* Do we need to switch interpreters? */
    JAL(MterpShouldSwitchInterpreters)
    bnez    v0, MterpFallback
    /* resume execution at catch block */
    EXPORT_PC()
    FETCH_INST()
    GET_INST_OPCODE(t0)
    GOTO_OPCODE(t0)
    /* NOTE: no fallthrough */

/*
 * Common handling for branches with support for Jit profiling.
 * On entry:
 *    rINST          <= signed offset
 *    rPROFILE       <= signed hotness countdown (expanded to 32 bits)
 *
 * We have quite a few different cases for branch profiling, OSR detection and
 * suspend check support here.
 *
 * Taken backward branches:
 *    If profiling active, do hotness countdown and report if we hit zero.
 *    If in osr check mode, see if our target is a compiled loop header entry and do OSR if so.
 *    Is there a pending suspend request?  If so, suspend.
 *
 * Taken forward branches and not-taken backward branches:
 *    If in osr check mode, see if our target is a compiled loop header entry and do OSR if so.
 *
 * Our most common case is expected to be a taken backward branch with active jit profiling,
 * but no full OSR check and no pending suspend request.
 * Next most common case is not-taken branch with no full OSR check.
 */
MterpCommonTakenBranchNoFlags:
    bgtz    rINST, .L_forward_branch    # don't add forward branches to hotness
/*
 * We need to subtract 1 from positive values and we should not see 0 here,
 * so we may use the result of the comparison with -1.
 */
#if JIT_CHECK_OSR != -1
#  error "JIT_CHECK_OSR must be -1."
#endif
    li      t0, JIT_CHECK_OSR
    beq     rPROFILE, t0, .L_osr_check
    blt     rPROFILE, t0, .L_resume_backward_branch
    subu    rPROFILE, 1
    beqz    rPROFILE, .L_add_batch      # counted down to zero - report
.L_resume_backward_branch:
    lw      ra, THREAD_FLAGS_OFFSET(rSELF)
    REFRESH_IBASE()
    addu    a2, rINST, rINST            # a2<- byte offset
    FETCH_ADVANCE_INST_RB(a2)           # update rPC, load rINST
    and     ra, THREAD_SUSPEND_OR_CHECKPOINT_REQUEST
    bnez    ra, .L_suspend_request_pending
    GET_INST_OPCODE(t0)                 # extract opcode from rINST
    GOTO_OPCODE(t0)                     # jump to next instruction

.L_suspend_request_pending:
    EXPORT_PC()
    move    a0, rSELF
    JAL(MterpSuspendCheck)              # (self)
    bnez    v0, MterpFallback
    REFRESH_IBASE()                     # might have changed during suspend
    GET_INST_OPCODE(t0)                 # extract opcode from rINST
    GOTO_OPCODE(t0)                     # jump to next instruction

.L_no_count_backwards:
    li      t0, JIT_CHECK_OSR           # check for possible OSR re-entry
    bne     rPROFILE, t0, .L_resume_backward_branch
.L_osr_check:
    move    a0, rSELF
    addu    a1, rFP, OFF_FP_SHADOWFRAME
    move    a2, rINST
    EXPORT_PC()
    JAL(MterpMaybeDoOnStackReplacement) # (self, shadow_frame, offset)
    bnez    v0, MterpOnStackReplacement
    b       .L_resume_backward_branch

.L_forward_branch:
    li      t0, JIT_CHECK_OSR           # check for possible OSR re-entry
    beq     rPROFILE, t0, .L_check_osr_forward
.L_resume_forward_branch:
    add     a2, rINST, rINST            # a2<- byte offset
    FETCH_ADVANCE_INST_RB(a2)           # update rPC, load rINST
    GET_INST_OPCODE(t0)                 # extract opcode from rINST
    GOTO_OPCODE(t0)                     # jump to next instruction

.L_check_osr_forward:
    move    a0, rSELF
    addu    a1, rFP, OFF_FP_SHADOWFRAME
    move    a2, rINST
    EXPORT_PC()
    JAL(MterpMaybeDoOnStackReplacement) # (self, shadow_frame, offset)
    bnez    v0, MterpOnStackReplacement
    b       .L_resume_forward_branch

.L_add_batch:
    addu    a1, rFP, OFF_FP_SHADOWFRAME
    sh      rPROFILE, SHADOWFRAME_HOTNESS_COUNTDOWN_OFFSET(a1)
    lw      a0, OFF_FP_METHOD(rFP)
    move    a2, rSELF
    JAL(MterpAddHotnessBatch)           # (method, shadow_frame, self)
    move    rPROFILE, v0                # restore new hotness countdown to rPROFILE
    b       .L_no_count_backwards

/*
 * Entered from the conditional branch handlers when OSR check request active on
 * not-taken path.  All Dalvik not-taken conditional branch offsets are 2.
 */
.L_check_not_taken_osr:
    move    a0, rSELF
    addu    a1, rFP, OFF_FP_SHADOWFRAME
    li      a2, 2
    EXPORT_PC()
    JAL(MterpMaybeDoOnStackReplacement) # (self, shadow_frame, offset)
    bnez    v0, MterpOnStackReplacement
    FETCH_ADVANCE_INST(2)
    GET_INST_OPCODE(t0)                 # extract opcode from rINST
    GOTO_OPCODE(t0)                     # jump to next instruction

/*
 * On-stack replacement has happened, and now we've returned from the compiled method.
 */
MterpOnStackReplacement:
#if MTERP_LOGGING
    move    a0, rSELF
    addu    a1, rFP, OFF_FP_SHADOWFRAME
    move    a2, rINST
    JAL(MterpLogOSR)
#endif
    li      v0, 1                       # Signal normal return
    b       MterpDone

/*
 * Bail out to reference interpreter.
 */
MterpFallback:
    EXPORT_PC()
#if MTERP_LOGGING
    move  a0, rSELF
    addu  a1, rFP, OFF_FP_SHADOWFRAME
    JAL(MterpLogFallback)
#endif
MterpCommonFallback:
    move    v0, zero                    # signal retry with reference interpreter.
    b       MterpDone
/*
 * We pushed some registers on the stack in ExecuteMterpImpl, then saved
 * SP and LR.  Here we restore SP, restore the registers, and then restore
 * LR to PC.
 *
 * On entry:
 *  uint32_t* rFP  (should still be live, pointer to base of vregs)
 */
MterpExceptionReturn:
    li      v0, 1                       # signal return to caller.
    b       MterpDone
MterpReturn:
    lw      a2, OFF_FP_RESULT_REGISTER(rFP)
    sw      v0, 0(a2)
    sw      v1, 4(a2)
    li      v0, 1                       # signal return to caller.
MterpDone:
/*
 * At this point, we expect rPROFILE to be non-zero.  If negative, hotness is disabled or we're
 * checking for OSR.  If greater than zero, we might have unreported hotness to register
 * (the difference between the ending rPROFILE and the cached hotness counter).  rPROFILE
 * should only reach zero immediately after a hotness decrement, and is then reset to either
 * a negative special state or the new non-zero countdown value.
 */
    blez    rPROFILE, .L_pop_and_return # if > 0, we may have some counts to report.

MterpProfileActive:
    move    rINST, v0                   # stash return value
    /* Report cached hotness counts */
    lw      a0, OFF_FP_METHOD(rFP)
    addu    a1, rFP, OFF_FP_SHADOWFRAME
    move    a2, rSELF
    sh      rPROFILE, SHADOWFRAME_HOTNESS_COUNTDOWN_OFFSET(a1)
    JAL(MterpAddHotnessBatch)           # (method, shadow_frame, self)
    move    v0, rINST                   # restore return value

.L_pop_and_return:
/* Restore from the stack and return. Frame size = STACK_SIZE */
    STACK_LOAD_FULL()
    jalr    zero, ra

    .cfi_endproc
    .end ExecuteMterpImpl

%def instruction_end():

    .global artMterpAsmInstructionEnd
artMterpAsmInstructionEnd:

%def instruction_end_alt():

    .global artMterpAsmAltInstructionEnd
artMterpAsmAltInstructionEnd:

%def instruction_start():

    .global artMterpAsmInstructionStart
artMterpAsmInstructionStart = .L_op_nop
    .text

%def instruction_start_alt():

    .global artMterpAsmAltInstructionStart
artMterpAsmAltInstructionStart = .L_ALT_op_nop
    .text

