/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright (c) 2021 Loongson Technology Corporation Limited
 */

static bool gen_rrr(DisasContext *ctx, arg_rrr *a,
                    DisasExtend src1_ext, DisasExtend src2_ext,
                    DisasExtend dst_ext, void (*func)(TCGv, TCGv, TCGv))
{
    TCGv dest = gpr_dst(ctx, a->rd, dst_ext);
    TCGv src1 = gpr_src(ctx, a->rj, src1_ext);
    TCGv src2 = gpr_src(ctx, a->rk, src2_ext);

    func(dest, src1, src2);
    gen_set_gpr(a->rd, dest, dst_ext);

    return true;
}

static bool gen_rri_v(DisasContext *ctx, arg_rr_i *a,
                      DisasExtend src_ext, DisasExtend dst_ext,
                      void (*func)(TCGv, TCGv, TCGv))
{
    TCGv dest = gpr_dst(ctx, a->rd, dst_ext);
    TCGv src1 = gpr_src(ctx, a->rj, src_ext);
    TCGv src2 = tcg_constant_tl(a->imm);

    func(dest, src1, src2);
    gen_set_gpr(a->rd, dest, dst_ext);

    return true;
}

static bool gen_rri_c(DisasContext *ctx, arg_rr_i *a,
                      DisasExtend src_ext, DisasExtend dst_ext,
                      void (*func)(TCGv, TCGv, target_long))
{
    TCGv dest = gpr_dst(ctx, a->rd, dst_ext);
    TCGv src1 = gpr_src(ctx, a->rj, src_ext);

    func(dest, src1, a->imm);
    gen_set_gpr(a->rd, dest, dst_ext);

    return true;
}

static bool gen_rrr_sa(DisasContext *ctx, arg_rrr_sa *a,
                       DisasExtend src_ext, DisasExtend dst_ext,
                       void (*func)(TCGv, TCGv, TCGv, target_long))
{
    TCGv dest = gpr_dst(ctx, a->rd, dst_ext);
    TCGv src1 = gpr_src(ctx, a->rj, src_ext);
    TCGv src2 = gpr_src(ctx, a->rk, src_ext);

    func(dest, src1, src2, a->sa);
    gen_set_gpr(a->rd, dest, dst_ext);

    return true;
}

static bool trans_lu12i_w(DisasContext *ctx, arg_lu12i_w *a)
{
    TCGv dest = gpr_dst(ctx, a->rd, EXT_NONE);

    tcg_gen_movi_tl(dest, a->imm << 12);
    gen_set_gpr(a->rd, dest, EXT_NONE);

    return true;
}

static bool gen_pc(DisasContext *ctx, arg_r_i *a,
                   target_ulong (*func)(target_ulong, int))
{
    TCGv dest = gpr_dst(ctx, a->rd, EXT_NONE);
    target_ulong addr = make_address_pc(ctx, func(ctx->base.pc_next, a->imm));

    tcg_gen_movi_tl(dest, addr);
    gen_set_gpr(a->rd, dest, EXT_NONE);

    return true;
}

static void gen_slt(TCGv dest, TCGv src1, TCGv src2)
{
    tcg_gen_setcond_tl(TCG_COND_LT, dest, src1, src2);
}

static void gen_sltu(TCGv dest, TCGv src1, TCGv src2)
{
    tcg_gen_setcond_tl(TCG_COND_LTU, dest, src1, src2);
}

static void gen_mulh_w(TCGv dest, TCGv src1, TCGv src2)
{
    tcg_gen_mul_i64(dest, src1, src2);
    tcg_gen_sari_i64(dest, dest, 32);
}

static void gen_mulh_d(TCGv dest, TCGv src1, TCGv src2)
{
    TCGv discard = tcg_temp_new();
    tcg_gen_muls2_tl(discard, dest, src1, src2);
}

static void gen_mulh_du(TCGv dest, TCGv src1, TCGv src2)
{
    TCGv discard = tcg_temp_new();
    tcg_gen_mulu2_tl(discard, dest, src1, src2);
}

static void prep_divisor_d(TCGv ret, TCGv src1, TCGv src2)
{
    TCGv t0 = tcg_temp_new();
    TCGv t1 = tcg_temp_new();
    TCGv zero = tcg_constant_tl(0);

    /*
     * If min / -1, set the divisor to 1.
     * This avoids potential host overflow trap and produces min.
     * If x / 0, set the divisor to 1.
     * This avoids potential host overflow trap;
     * the required result is undefined.
     */
    tcg_gen_setcondi_tl(TCG_COND_EQ, ret, src1, INT64_MIN);
    tcg_gen_setcondi_tl(TCG_COND_EQ, t0, src2, -1);
    tcg_gen_setcondi_tl(TCG_COND_EQ, t1, src2, 0);
    tcg_gen_and_tl(ret, ret, t0);
    tcg_gen_or_tl(ret, ret, t1);
    tcg_gen_movcond_tl(TCG_COND_NE, ret, ret, zero, ret, src2);
}

static void prep_divisor_du(TCGv ret, TCGv src2)
{
    TCGv zero = tcg_constant_tl(0);
    TCGv one = tcg_constant_tl(1);

    /*
     * If x / 0, set the divisor to 1.
     * This avoids potential host overflow trap;
     * the required result is undefined.
     */
    tcg_gen_movcond_tl(TCG_COND_EQ, ret, src2, zero, one, src2);
}

static void gen_div_d(TCGv dest, TCGv src1, TCGv src2)
{
    TCGv t0 = tcg_temp_new();
    prep_divisor_d(t0, src1, src2);
    tcg_gen_div_tl(dest, src1, t0);
}

static void gen_rem_d(TCGv dest, TCGv src1, TCGv src2)
{
    TCGv t0 = tcg_temp_new();
    prep_divisor_d(t0, src1, src2);
    tcg_gen_rem_tl(dest, src1, t0);
}

static void gen_div_du(TCGv dest, TCGv src1, TCGv src2)
{
    TCGv t0 = tcg_temp_new();
    prep_divisor_du(t0, src2);
    tcg_gen_divu_tl(dest, src1, t0);
}

static void gen_rem_du(TCGv dest, TCGv src1, TCGv src2)
{
    TCGv t0 = tcg_temp_new();
    prep_divisor_du(t0, src2);
    tcg_gen_remu_tl(dest, src1, t0);
}

static void gen_div_w(TCGv dest, TCGv src1, TCGv src2)
{
    TCGv t0 = tcg_temp_new();
    /* We need not check for integer overflow for div_w. */
    prep_divisor_du(t0, src2);
    tcg_gen_div_tl(dest, src1, t0);
}

static void gen_rem_w(TCGv dest, TCGv src1, TCGv src2)
{
    TCGv t0 = tcg_temp_new();
    /* We need not check for integer overflow for rem_w. */
    prep_divisor_du(t0, src2);
    tcg_gen_rem_tl(dest, src1, t0);
}

static void gen_alsl(TCGv dest, TCGv src1, TCGv src2, target_long sa)
{
    TCGv t0 = tcg_temp_new();
    tcg_gen_shli_tl(t0, src1, sa);
    tcg_gen_add_tl(dest, t0, src2);
}

static bool trans_lu32i_d(DisasContext *ctx, arg_lu32i_d *a)
{
    TCGv dest = gpr_dst(ctx, a->rd, EXT_NONE);
    TCGv src1 = gpr_src(ctx, a->rd, EXT_NONE);
    TCGv src2 = tcg_constant_tl(a->imm);

    if (!avail_64(ctx)) {
        return false;
    }

    tcg_gen_deposit_tl(dest, src1, src2, 32, 32);
    gen_set_gpr(a->rd, dest, EXT_NONE);

    return true;
}

static bool trans_lu52i_d(DisasContext *ctx, arg_lu52i_d *a)
{
    TCGv dest = gpr_dst(ctx, a->rd, EXT_NONE);
    TCGv src1 = gpr_src(ctx, a->rj, EXT_NONE);
    TCGv src2 = tcg_constant_tl(a->imm);

    if (!avail_64(ctx)) {
        return false;
    }

    tcg_gen_deposit_tl(dest, src1, src2, 52, 12);
    gen_set_gpr(a->rd, dest, EXT_NONE);

    return true;
}

static target_ulong gen_pcaddi(target_ulong pc, int imm)
{
    return pc + (imm << 2);
}

static target_ulong gen_pcalau12i(target_ulong pc, int imm)
{
    return (pc + (imm << 12)) & ~0xfff;
}

static target_ulong gen_pcaddu12i(target_ulong pc, int imm)
{
    return pc + (imm << 12);
}

static target_ulong gen_pcaddu18i(target_ulong pc, int imm)
{
    return pc + ((target_ulong)(imm) << 18);
}

static bool trans_addu16i_d(DisasContext *ctx, arg_addu16i_d *a)
{
    TCGv dest = gpr_dst(ctx, a->rd, EXT_NONE);
    TCGv src1 = gpr_src(ctx, a->rj, EXT_NONE);

    if (!avail_64(ctx)) {
        return false;
    }

    tcg_gen_addi_tl(dest, src1, a->imm << 16);
    gen_set_gpr(a->rd, dest, EXT_NONE);

    return true;
}

TRANS(add_w, ALL, gen_rrr, EXT_NONE, EXT_NONE, EXT_SIGN, tcg_gen_add_tl)
TRANS(add_d, 64, gen_rrr, EXT_NONE, EXT_NONE, EXT_NONE, tcg_gen_add_tl)
TRANS(sub_w, ALL, gen_rrr, EXT_NONE, EXT_NONE, EXT_SIGN, tcg_gen_sub_tl)
TRANS(sub_d, 64, gen_rrr, EXT_NONE, EXT_NONE, EXT_NONE, tcg_gen_sub_tl)
TRANS(and, ALL, gen_rrr, EXT_NONE, EXT_NONE, EXT_NONE, tcg_gen_and_tl)
TRANS(or, ALL, gen_rrr, EXT_NONE, EXT_NONE, EXT_NONE, tcg_gen_or_tl)
TRANS(xor, ALL, gen_rrr, EXT_NONE, EXT_NONE, EXT_NONE, tcg_gen_xor_tl)
TRANS(nor, ALL, gen_rrr, EXT_NONE, EXT_NONE, EXT_NONE, tcg_gen_nor_tl)
TRANS(andn, ALL, gen_rrr, EXT_NONE, EXT_NONE, EXT_NONE, tcg_gen_andc_tl)
TRANS(orn, ALL, gen_rrr, EXT_NONE, EXT_NONE, EXT_NONE, tcg_gen_orc_tl)
TRANS(slt, ALL, gen_rrr, EXT_NONE, EXT_NONE, EXT_NONE, gen_slt)
TRANS(sltu, ALL, gen_rrr, EXT_NONE, EXT_NONE, EXT_NONE, gen_sltu)
TRANS(mul_w, ALL, gen_rrr, EXT_SIGN, EXT_SIGN, EXT_SIGN, tcg_gen_mul_tl)
TRANS(mul_d, 64, gen_rrr, EXT_NONE, EXT_NONE, EXT_NONE, tcg_gen_mul_tl)
TRANS(mulh_w, ALL, gen_rrr, EXT_SIGN, EXT_SIGN, EXT_NONE, gen_mulh_w)
TRANS(mulh_wu, ALL, gen_rrr, EXT_ZERO, EXT_ZERO, EXT_NONE, gen_mulh_w)
TRANS(mulh_d, 64, gen_rrr, EXT_NONE, EXT_NONE, EXT_NONE, gen_mulh_d)
TRANS(mulh_du, 64, gen_rrr, EXT_NONE, EXT_NONE, EXT_NONE, gen_mulh_du)
TRANS(mulw_d_w, 64, gen_rrr, EXT_SIGN, EXT_SIGN, EXT_NONE, tcg_gen_mul_tl)
TRANS(mulw_d_wu, 64, gen_rrr, EXT_ZERO, EXT_ZERO, EXT_NONE, tcg_gen_mul_tl)
TRANS(div_w, ALL, gen_rrr, EXT_SIGN, EXT_SIGN, EXT_SIGN, gen_div_w)
TRANS(mod_w, ALL, gen_rrr, EXT_SIGN, EXT_SIGN, EXT_SIGN, gen_rem_w)
TRANS(div_wu, ALL, gen_rrr, EXT_ZERO, EXT_ZERO, EXT_SIGN, gen_div_du)
TRANS(mod_wu, ALL, gen_rrr, EXT_ZERO, EXT_ZERO, EXT_SIGN, gen_rem_du)
TRANS(div_d, 64, gen_rrr, EXT_NONE, EXT_NONE, EXT_NONE, gen_div_d)
TRANS(mod_d, 64, gen_rrr, EXT_NONE, EXT_NONE, EXT_NONE, gen_rem_d)
TRANS(div_du, 64, gen_rrr, EXT_NONE, EXT_NONE, EXT_NONE, gen_div_du)
TRANS(mod_du, 64, gen_rrr, EXT_NONE, EXT_NONE, EXT_NONE, gen_rem_du)
TRANS(slti, ALL, gen_rri_v, EXT_NONE, EXT_NONE, gen_slt)
TRANS(sltui, ALL, gen_rri_v, EXT_NONE, EXT_NONE, gen_sltu)
TRANS(addi_w, ALL, gen_rri_c, EXT_NONE, EXT_SIGN, tcg_gen_addi_tl)
TRANS(addi_d, 64, gen_rri_c, EXT_NONE, EXT_NONE, tcg_gen_addi_tl)
TRANS(alsl_w, ALL, gen_rrr_sa, EXT_NONE, EXT_SIGN, gen_alsl)
TRANS(alsl_wu, 64, gen_rrr_sa, EXT_NONE, EXT_ZERO, gen_alsl)
TRANS(alsl_d, 64, gen_rrr_sa, EXT_NONE, EXT_NONE, gen_alsl)
TRANS(pcaddi, ALL, gen_pc, gen_pcaddi)
TRANS(pcalau12i, ALL, gen_pc, gen_pcalau12i)
TRANS(pcaddu12i, ALL, gen_pc, gen_pcaddu12i)
TRANS(pcaddu18i, 64, gen_pc, gen_pcaddu18i)
TRANS(andi, ALL, gen_rri_c, EXT_NONE, EXT_NONE, tcg_gen_andi_tl)
TRANS(ori, ALL, gen_rri_c, EXT_NONE, EXT_NONE, tcg_gen_ori_tl)
TRANS(xori, ALL, gen_rri_c, EXT_NONE, EXT_NONE, tcg_gen_xori_tl)
