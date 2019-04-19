/*
 *  Header file for wrappers around MSA instructions assembler invocations
 *
 *  Copyright (C) 2019  Wave Computing, Inc.
 *  Copyright (C) 2019  Aleksandar Markovic <amarkovic@wavecomp.com>
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 */

#ifndef WRAPPERS_MSA_H
#define WRAPPERS_MSA_H


#define DO_MSA__WD__WS(suffix, mnemonic)                               \
static inline void do_msa_##suffix(const void *input,                  \
                                   const void *output)                 \
{                                                                      \
   __asm__ volatile (                                                  \
      "move $t0, %0\n\t"                                               \
      "ld.d $w11, 0($t0)\n\t"                                          \
      #mnemonic " $w10, $w11\n\t"                                      \
      "move $t0, %1\n\t"                                               \
      "st.d $w10, 0($t0)\n\t"                                          \
      :                                                                \
      : "r" (input), "r" (output)                                      \
      : "t0", "memory"                                                 \
   );                                                                  \
}

#define DO_MSA__WD__WD(suffix, mnemonic)                               \
static inline void do_msa_##suffix(const void *input,                  \
                                   const void *output)                 \
{                                                                      \
   __asm__ volatile (                                                  \
      "move $t0, %0\n\t"                                               \
      "ld.d $w11, 0($t0)\n\t"                                          \
      #mnemonic " $w10, $w10\n\t"                                      \
      "move $t0, %1\n\t"                                               \
      "st.d $w10, 0($t0)\n\t"                                          \
      :                                                                \
      : "r" (input), "r" (output)                                      \
      : "t0", "memory"                                                 \
   );                                                                  \
}

DO_MSA__WD__WS(NLOC_B, nloc.b)
DO_MSA__WD__WS(NLOC_H, nloc.h)
DO_MSA__WD__WS(NLOC_W, nloc.w)
DO_MSA__WD__WS(NLOC_D, nloc.d)

DO_MSA__WD__WS(NLZC_B, nlzc.b)
DO_MSA__WD__WS(NLZC_H, nlzc.h)
DO_MSA__WD__WS(NLZC_W, nlzc.w)
DO_MSA__WD__WS(NLZC_D, nlzc.d)

DO_MSA__WD__WS(PCNT_B, pcnt.b)
DO_MSA__WD__WS(PCNT_H, pcnt.h)
DO_MSA__WD__WS(PCNT_W, pcnt.w)
DO_MSA__WD__WS(PCNT_D, pcnt.d)


#define DO_MSA__WD__WS_WT(suffix, mnemonic)                            \
static inline void do_msa_##suffix(const void *input1,                 \
                                   const void *input2,                 \
                                   const void *output)                 \
{                                                                      \
   __asm__ volatile (                                                  \
      "move $t0, %0\n\t"                                               \
      "ld.d $w11, 0($t0)\n\t"                                          \
      "move $t0, %1\n\t"                                               \
      "ld.d $w12, 0($t0)\n\t"                                          \
      #mnemonic " $w10, $w11, $w12\n\t"                                \
      "move $t0, %2\n\t"                                               \
      "st.d $w10, 0($t0)\n\t"                                          \
      :                                                                \
      : "r" (input1), "r" (input2), "r" (output)                       \
      : "t0", "memory"                                                 \
   );                                                                  \
}

#define DO_MSA__WD__WD_WT(suffix, mnemonic)                            \
static inline void do_msa_##suffix(void *input1, void *input2,         \
                                   void *output)                       \
{                                                                      \
   __asm__ volatile (                                                  \
      "move $t0, %0\n\t"                                               \
      "ld.d $w11, 0($t0)\n\t"                                          \
      "move $t0, %1\n\t"                                               \
      "ld.d $w12, 0($t0)\n\t"                                          \
      #mnemonic " $w10, $w10, $w12\n\t"                                \
      "move $t0, %2\n\t"                                               \
      "st.d $w10, 0($t0)\n\t"                                          \
      :                                                                \
      : "r" (input1), "r" (input2), "r" (output)                       \
      : "t0", "memory"                                                 \
   );                                                                  \
}

#define DO_MSA__WD__WS_WD(suffix, mnemonic)                            \
static inline void do_msa_##suffix(void *input1, void *input2,         \
                                   void *output)                       \
{                                                                      \
   __asm__ volatile (                                                  \
      "move $t0, %0\n\t"                                               \
      "ld.d $w11, 0($t0)\n\t"                                          \
      "move $t0, %1\n\t"                                               \
      "ld.d $w12, 0($t0)\n\t"                                          \
      #mnemonic " $w10, $w11, $w10\n\t"                                \
      "move $t0, %2\n\t"                                               \
      "st.d $w10, 0($t0)\n\t"                                          \
      :                                                                \
      : "r" (input1), "r" (input2), "r" (output)                       \
      : "t0", "memory"                                                 \
   );                                                                  \
}

DO_MSA__WD__WS_WT(ILVEV_B, ilvev.b)
DO_MSA__WD__WS_WT(ILVEV_H, ilvev.h)
DO_MSA__WD__WS_WT(ILVEV_W, ilvev.w)
DO_MSA__WD__WS_WT(ILVEV_D, ilvev.d)

DO_MSA__WD__WS_WT(ILVOD_B, ilvod.b)
DO_MSA__WD__WS_WT(ILVOD_H, ilvod.h)
DO_MSA__WD__WS_WT(ILVOD_W, ilvod.w)
DO_MSA__WD__WS_WT(ILVOD_D, ilvod.d)

DO_MSA__WD__WS_WT(ILVL_B, ilvl.b)
DO_MSA__WD__WS_WT(ILVL_H, ilvl.h)
DO_MSA__WD__WS_WT(ILVL_W, ilvl.w)
DO_MSA__WD__WS_WT(ILVL_D, ilvl.d)

DO_MSA__WD__WS_WT(ILVR_B, ilvr.b)
DO_MSA__WD__WS_WT(ILVR_H, ilvr.h)
DO_MSA__WD__WS_WT(ILVR_W, ilvr.w)
DO_MSA__WD__WS_WT(ILVR_D, ilvr.d)

DO_MSA__WD__WS_WT(AND_V, and.v)
DO_MSA__WD__WS_WT(NOR_V, nor.v)
DO_MSA__WD__WS_WT(OR_V, or.v)
DO_MSA__WD__WS_WT(XOR_V, xor.v)

DO_MSA__WD__WS_WT(CEQ_B, ceq.b)
DO_MSA__WD__WS_WT(CEQ_H, ceq.h)
DO_MSA__WD__WS_WT(CEQ_W, ceq.w)
DO_MSA__WD__WS_WT(CEQ_D, ceq.d)

DO_MSA__WD__WS_WT(CLE_S_B, cle_s.b)
DO_MSA__WD__WS_WT(CLE_S_H, cle_s.h)
DO_MSA__WD__WS_WT(CLE_S_W, cle_s.w)
DO_MSA__WD__WS_WT(CLE_S_D, cle_s.d)

DO_MSA__WD__WS_WT(CLE_U_B, cle_u.b)
DO_MSA__WD__WS_WT(CLE_U_H, cle_u.h)
DO_MSA__WD__WS_WT(CLE_U_W, cle_u.w)
DO_MSA__WD__WS_WT(CLE_U_D, cle_u.d)

DO_MSA__WD__WS_WT(CLT_S_B, clt_s.b)
DO_MSA__WD__WS_WT(CLT_S_H, clt_s.h)
DO_MSA__WD__WS_WT(CLT_S_W, clt_s.w)
DO_MSA__WD__WS_WT(CLT_S_D, clt_s.d)

DO_MSA__WD__WS_WT(CLT_U_B, clt_u.b)
DO_MSA__WD__WS_WT(CLT_U_H, clt_u.h)
DO_MSA__WD__WS_WT(CLT_U_W, clt_u.w)
DO_MSA__WD__WS_WT(CLT_U_D, clt_u.d)

DO_MSA__WD__WS_WT(MAX_A_B, max_a.b)
DO_MSA__WD__WS_WT(MAX_A_H, max_a.h)
DO_MSA__WD__WS_WT(MAX_A_W, max_a.w)
DO_MSA__WD__WS_WT(MAX_A_D, max_a.d)

DO_MSA__WD__WS_WT(MIN_A_B, min_a.b)
DO_MSA__WD__WS_WT(MIN_A_H, min_a.h)
DO_MSA__WD__WS_WT(MIN_A_W, min_a.w)
DO_MSA__WD__WS_WT(MIN_A_D, min_a.d)

DO_MSA__WD__WS_WT(MAX_S_B, max_s.b)
DO_MSA__WD__WS_WT(MAX_S_H, max_s.h)
DO_MSA__WD__WS_WT(MAX_S_W, max_s.w)
DO_MSA__WD__WS_WT(MAX_S_D, max_s.d)

DO_MSA__WD__WS_WT(MIN_S_B, min_s.b)
DO_MSA__WD__WS_WT(MIN_S_H, min_s.h)
DO_MSA__WD__WS_WT(MIN_S_W, min_s.w)
DO_MSA__WD__WS_WT(MIN_S_D, min_s.d)

DO_MSA__WD__WS_WT(MAX_U_B, max_u.b)
DO_MSA__WD__WS_WT(MAX_U_H, max_u.h)
DO_MSA__WD__WS_WT(MAX_U_W, max_u.w)
DO_MSA__WD__WS_WT(MAX_U_D, max_u.d)

DO_MSA__WD__WS_WT(MIN_U_B, min_u.b)
DO_MSA__WD__WS_WT(MIN_U_H, min_u.h)
DO_MSA__WD__WS_WT(MIN_U_W, min_u.w)
DO_MSA__WD__WS_WT(MIN_U_D, min_u.d)

DO_MSA__WD__WS_WT(BCLR_B, bclr.b)
DO_MSA__WD__WS_WT(BCLR_H, bclr.h)
DO_MSA__WD__WS_WT(BCLR_W, bclr.w)
DO_MSA__WD__WS_WT(BCLR_D, bclr.d)

DO_MSA__WD__WS_WT(BSET_B, bset.b)
DO_MSA__WD__WS_WT(BSET_H, bset.h)
DO_MSA__WD__WS_WT(BSET_W, bset.w)
DO_MSA__WD__WS_WT(BSET_D, bset.d)

DO_MSA__WD__WS_WT(BNEG_B, bneg.b)
DO_MSA__WD__WS_WT(BNEG_H, bneg.h)
DO_MSA__WD__WS_WT(BNEG_W, bneg.w)
DO_MSA__WD__WS_WT(BNEG_D, bneg.d)

DO_MSA__WD__WS_WT(PCKEV_B, pckev.b)
DO_MSA__WD__WS_WT(PCKEV_H, pckev.h)
DO_MSA__WD__WS_WT(PCKEV_W, pckev.w)
DO_MSA__WD__WS_WT(PCKEV_D, pckev.d)

DO_MSA__WD__WS_WT(PCKOD_B, pckod.b)
DO_MSA__WD__WS_WT(PCKOD_H, pckod.h)
DO_MSA__WD__WS_WT(PCKOD_W, pckod.w)
DO_MSA__WD__WS_WT(PCKOD_D, pckod.d)

DO_MSA__WD__WS_WT(VSHF_B, vshf.b)
DO_MSA__WD__WS_WT(VSHF_H, vshf.h)
DO_MSA__WD__WS_WT(VSHF_W, vshf.w)
DO_MSA__WD__WS_WT(VSHF_D, vshf.d)

DO_MSA__WD__WS_WT(SLL_B, sll.b)
DO_MSA__WD__WS_WT(SLL_H, sll.h)
DO_MSA__WD__WS_WT(SLL_W, sll.w)
DO_MSA__WD__WS_WT(SLL_D, sll.d)

DO_MSA__WD__WS_WT(SRA_B, sra.b)
DO_MSA__WD__WS_WT(SRA_H, sra.h)
DO_MSA__WD__WS_WT(SRA_W, sra.w)
DO_MSA__WD__WS_WT(SRA_D, sra.d)

DO_MSA__WD__WS_WT(SRAR_B, srar.b)
DO_MSA__WD__WS_WT(SRAR_H, srar.h)
DO_MSA__WD__WS_WT(SRAR_W, srar.w)
DO_MSA__WD__WS_WT(SRAR_D, srar.d)

DO_MSA__WD__WS_WT(SRL_B, srl.b)
DO_MSA__WD__WS_WT(SRL_H, srl.h)
DO_MSA__WD__WS_WT(SRL_W, srl.w)
DO_MSA__WD__WS_WT(SRL_D, srl.d)

DO_MSA__WD__WS_WT(SRLR_B, srlr.b)
DO_MSA__WD__WS_WT(SRLR_H, srlr.h)
DO_MSA__WD__WS_WT(SRLR_W, srlr.w)
DO_MSA__WD__WS_WT(SRLR_D, srlr.d)

DO_MSA__WD__WS_WT(ADD_A_B, add_a.b)
DO_MSA__WD__WS_WT(ADD_A_H, add_a.h)
DO_MSA__WD__WS_WT(ADD_A_W, add_a.w)
DO_MSA__WD__WS_WT(ADD_A_D, add_a.d)

DO_MSA__WD__WS_WT(ADDS_A_B, adds_a.b)
DO_MSA__WD__WS_WT(ADDS_A_H, adds_a.h)
DO_MSA__WD__WS_WT(ADDS_A_W, adds_a.w)
DO_MSA__WD__WS_WT(ADDS_A_D, adds_a.d)

DO_MSA__WD__WS_WT(ADDS_S_B, adds_s.b)
DO_MSA__WD__WS_WT(ADDS_S_H, adds_s.h)
DO_MSA__WD__WS_WT(ADDS_S_W, adds_s.w)
DO_MSA__WD__WS_WT(ADDS_S_D, adds_s.d)

DO_MSA__WD__WS_WT(ADDS_U_B, adds_u.b)
DO_MSA__WD__WS_WT(ADDS_U_H, adds_u.h)
DO_MSA__WD__WS_WT(ADDS_U_W, adds_u.w)
DO_MSA__WD__WS_WT(ADDS_U_D, adds_u.d)

DO_MSA__WD__WS_WT(ADDV_B, addv.b)
DO_MSA__WD__WS_WT(ADDV_H, addv.h)
DO_MSA__WD__WS_WT(ADDV_W, addv.w)
DO_MSA__WD__WS_WT(ADDV_D, addv.d)

DO_MSA__WD__WS_WT(HADD_S_H, hadd_s.h)
DO_MSA__WD__WS_WT(HADD_S_W, hadd_s.w)
DO_MSA__WD__WS_WT(HADD_S_D, hadd_s.d)

DO_MSA__WD__WS_WT(HADD_U_H, hadd_u.h)
DO_MSA__WD__WS_WT(HADD_U_W, hadd_u.w)
DO_MSA__WD__WS_WT(HADD_U_D, hadd_u.d)

DO_MSA__WD__WS_WT(AVER_S_B, aver_s.b)
DO_MSA__WD__WS_WT(AVER_S_H, aver_s.h)
DO_MSA__WD__WS_WT(AVER_S_W, aver_s.w)
DO_MSA__WD__WS_WT(AVER_S_D, aver_s.d)

DO_MSA__WD__WS_WT(AVER_U_B, aver_u.b)
DO_MSA__WD__WS_WT(AVER_U_H, aver_u.h)
DO_MSA__WD__WS_WT(AVER_U_W, aver_u.w)
DO_MSA__WD__WS_WT(AVER_U_D, aver_u.d)

DO_MSA__WD__WS_WT(AVE_S_B, ave_s.b)
DO_MSA__WD__WS_WT(AVE_S_H, ave_s.h)
DO_MSA__WD__WS_WT(AVE_S_W, ave_s.w)
DO_MSA__WD__WS_WT(AVE_S_D, ave_s.d)

DO_MSA__WD__WS_WT(AVE_U_B, ave_u.b)
DO_MSA__WD__WS_WT(AVE_U_H, ave_u.h)
DO_MSA__WD__WS_WT(AVE_U_W, ave_u.w)
DO_MSA__WD__WS_WT(AVE_U_D, ave_u.d)

DO_MSA__WD__WS_WT(DIV_S_B, div_s.b)
DO_MSA__WD__WS_WT(DIV_S_H, div_s.h)
DO_MSA__WD__WS_WT(DIV_S_W, div_s.w)
DO_MSA__WD__WS_WT(DIV_S_D, div_s.d)

DO_MSA__WD__WS_WT(DIV_U_B, div_u.b)
DO_MSA__WD__WS_WT(DIV_U_H, div_u.h)
DO_MSA__WD__WS_WT(DIV_U_W, div_u.w)
DO_MSA__WD__WS_WT(DIV_U_D, div_u.d)

DO_MSA__WD__WS_WT(DOTP_S_H, dotp_s.h)
DO_MSA__WD__WS_WT(DOTP_S_W, dotp_s.w)
DO_MSA__WD__WS_WT(DOTP_S_D, dotp_s.d)

DO_MSA__WD__WS_WT(DOTP_U_H, dotp_u.h)
DO_MSA__WD__WS_WT(DOTP_U_W, dotp_u.w)
DO_MSA__WD__WS_WT(DOTP_U_D, dotp_u.d)

DO_MSA__WD__WS_WT(MOD_S_B, mod_s.b)
DO_MSA__WD__WS_WT(MOD_S_H, mod_s.h)
DO_MSA__WD__WS_WT(MOD_S_W, mod_s.w)
DO_MSA__WD__WS_WT(MOD_S_D, mod_s.d)

DO_MSA__WD__WS_WT(MOD_U_B, mod_u.b)
DO_MSA__WD__WS_WT(MOD_U_H, mod_u.h)
DO_MSA__WD__WS_WT(MOD_U_W, mod_u.w)
DO_MSA__WD__WS_WT(MOD_U_D, mod_u.d)

DO_MSA__WD__WS_WT(MUL_Q_H, mul_q.h)
DO_MSA__WD__WS_WT(MUL_Q_W, mul_q.w)
DO_MSA__WD__WS_WT(MULR_Q_H, mulr_q.h)
DO_MSA__WD__WS_WT(MULR_Q_W, mulr_q.w)

DO_MSA__WD__WS_WT(MULV_B, mulv.b)
DO_MSA__WD__WS_WT(MULV_H, mulv.h)
DO_MSA__WD__WS_WT(MULV_W, mulv.w)
DO_MSA__WD__WS_WT(MULV_D, mulv.d)

DO_MSA__WD__WS_WT(SUBV_B, subv.b)
DO_MSA__WD__WS_WT(SUBV_H, subv.h)
DO_MSA__WD__WS_WT(SUBV_W, subv.w)
DO_MSA__WD__WS_WT(SUBV_D, subv.d)

DO_MSA__WD__WS_WT(SUBS_S_B, subs_s.b)
DO_MSA__WD__WS_WT(SUBS_S_H, subs_s.h)
DO_MSA__WD__WS_WT(SUBS_S_W, subs_s.w)
DO_MSA__WD__WS_WT(SUBS_S_D, subs_s.d)

DO_MSA__WD__WS_WT(SUBS_U_B, subs_u.b)
DO_MSA__WD__WS_WT(SUBS_U_H, subs_u.h)
DO_MSA__WD__WS_WT(SUBS_U_W, subs_u.w)
DO_MSA__WD__WS_WT(SUBS_U_D, subs_u.d)

DO_MSA__WD__WS_WT(ASUB_S_B, asub_s.b)
DO_MSA__WD__WS_WT(ASUB_S_H, asub_s.h)
DO_MSA__WD__WS_WT(ASUB_S_W, asub_s.w)
DO_MSA__WD__WS_WT(ASUB_S_D, asub_s.d)

DO_MSA__WD__WS_WT(ASUB_U_B, asub_u.b)
DO_MSA__WD__WS_WT(ASUB_U_H, asub_u.h)
DO_MSA__WD__WS_WT(ASUB_U_W, asub_u.w)
DO_MSA__WD__WS_WT(ASUB_U_D, asub_u.d)

DO_MSA__WD__WS_WT(SUBSUU_S_B, subsuu_s.b)
DO_MSA__WD__WS_WT(SUBSUU_S_H, subsuu_s.h)
DO_MSA__WD__WS_WT(SUBSUU_S_W, subsuu_s.w)
DO_MSA__WD__WS_WT(SUBSUU_S_D, subsuu_s.d)

DO_MSA__WD__WS_WT(SUBSUS_U_B, subsus_u.b)
DO_MSA__WD__WS_WT(SUBSUS_U_H, subsus_u.h)
DO_MSA__WD__WS_WT(SUBSUS_U_W, subsus_u.w)
DO_MSA__WD__WS_WT(SUBSUS_U_D, subsus_u.d)

DO_MSA__WD__WS_WT(HSUB_S_H, hsub_s.h)
DO_MSA__WD__WS_WT(HSUB_S_W, hsub_s.w)
DO_MSA__WD__WS_WT(HSUB_S_D, hsub_s.d)

DO_MSA__WD__WS_WT(HSUB_U_H, hsub_u.h)
DO_MSA__WD__WS_WT(HSUB_U_W, hsub_u.w)
DO_MSA__WD__WS_WT(HSUB_U_D, hsub_u.d)



DO_MSA__WD__WS_WT(BMNZ_V, bmnz.v)
DO_MSA__WD__WS_WT(BMZ_V, bmz.v)

DO_MSA__WD__WS_WT(FMAX_W, fmax.w)
DO_MSA__WD__WS_WT(FMAX_D, fmax.d)

DO_MSA__WD__WS_WT(FMAX_A_W, fmax_a.w)
DO_MSA__WD__WS_WT(FMAX_A_D, fmax_a.d)

DO_MSA__WD__WS_WT(FMIN_W, fmin.w)
DO_MSA__WD__WS_WT(FMIN_D, fmin.d)

DO_MSA__WD__WS_WT(FMIN_A_W, fmin_a.w)
DO_MSA__WD__WS_WT(FMIN_A_D, fmin_a.d)


#endif
