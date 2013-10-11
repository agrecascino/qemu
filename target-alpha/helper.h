#include "exec/def-helper.h"

DEF_HELPER_3(excp, noreturn, env, int, int)
DEF_HELPER_FLAGS_1(load_pcc, TCG_CALL_NO_RWG_SE, i64, env)

DEF_HELPER_FLAGS_3(addqv, TCG_CALL_NO_WG, i64, env, i64, i64)
DEF_HELPER_FLAGS_3(addlv, TCG_CALL_NO_WG, i64, env, i64, i64)
DEF_HELPER_FLAGS_3(subqv, TCG_CALL_NO_WG, i64, env, i64, i64)
DEF_HELPER_FLAGS_3(sublv, TCG_CALL_NO_WG, i64, env, i64, i64)
DEF_HELPER_FLAGS_3(mullv, TCG_CALL_NO_WG, i64, env, i64, i64)
DEF_HELPER_FLAGS_3(mulqv, TCG_CALL_NO_WG, i64, env, i64, i64)

DEF_HELPER_FLAGS_1(ctpop, TCG_CALL_NO_RWG_SE, i64, i64)
DEF_HELPER_FLAGS_1(ctlz, TCG_CALL_NO_RWG_SE, i64, i64)
DEF_HELPER_FLAGS_1(cttz, TCG_CALL_NO_RWG_SE, i64, i64)

DEF_HELPER_FLAGS_2(zap, TCG_CALL_NO_RWG_SE, i64, i64, i64)
DEF_HELPER_FLAGS_2(zapnot, TCG_CALL_NO_RWG_SE, i64, i64, i64)

DEF_HELPER_FLAGS_2(cmpbge, TCG_CALL_NO_RWG_SE, i64, i64, i64)

DEF_HELPER_FLAGS_2(minub8, TCG_CALL_NO_RWG_SE, i64, i64, i64)
DEF_HELPER_FLAGS_2(minsb8, TCG_CALL_NO_RWG_SE, i64, i64, i64)
DEF_HELPER_FLAGS_2(minuw4, TCG_CALL_NO_RWG_SE, i64, i64, i64)
DEF_HELPER_FLAGS_2(minsw4, TCG_CALL_NO_RWG_SE, i64, i64, i64)
DEF_HELPER_FLAGS_2(maxub8, TCG_CALL_NO_RWG_SE, i64, i64, i64)
DEF_HELPER_FLAGS_2(maxsb8, TCG_CALL_NO_RWG_SE, i64, i64, i64)
DEF_HELPER_FLAGS_2(maxuw4, TCG_CALL_NO_RWG_SE, i64, i64, i64)
DEF_HELPER_FLAGS_2(maxsw4, TCG_CALL_NO_RWG_SE, i64, i64, i64)
DEF_HELPER_FLAGS_2(perr, TCG_CALL_NO_RWG_SE, i64, i64, i64)
DEF_HELPER_FLAGS_1(pklb, TCG_CALL_NO_RWG_SE, i64, i64)
DEF_HELPER_FLAGS_1(pkwb, TCG_CALL_NO_RWG_SE, i64, i64)
DEF_HELPER_FLAGS_1(unpkbl, TCG_CALL_NO_RWG_SE, i64, i64)
DEF_HELPER_FLAGS_1(unpkbw, TCG_CALL_NO_RWG_SE, i64, i64)

DEF_HELPER_FLAGS_1(load_fpcr, TCG_CALL_NO_RWG_SE, i64, env)
DEF_HELPER_FLAGS_2(store_fpcr, TCG_CALL_NO_RWG, void, env, i64)

DEF_HELPER_FLAGS_1(f_to_memory, TCG_CALL_NO_RWG_SE, i32, i64)
DEF_HELPER_FLAGS_1(memory_to_f, TCG_CALL_NO_RWG_SE, i64, i32)
DEF_HELPER_FLAGS_3(addf, TCG_CALL_NO_RWG, i64, env, i64, i64)
DEF_HELPER_FLAGS_3(subf, TCG_CALL_NO_RWG, i64, env, i64, i64)
DEF_HELPER_FLAGS_3(mulf, TCG_CALL_NO_RWG, i64, env, i64, i64)
DEF_HELPER_FLAGS_3(divf, TCG_CALL_NO_RWG, i64, env, i64, i64)
DEF_HELPER_FLAGS_2(sqrtf, TCG_CALL_NO_RWG, i64, env, i64)

DEF_HELPER_FLAGS_1(g_to_memory, TCG_CALL_NO_RWG_SE, i64, i64)
DEF_HELPER_FLAGS_1(memory_to_g, TCG_CALL_NO_RWG_SE, i64, i64)
DEF_HELPER_FLAGS_3(addg, TCG_CALL_NO_RWG, i64, env, i64, i64)
DEF_HELPER_FLAGS_3(subg, TCG_CALL_NO_RWG, i64, env, i64, i64)
DEF_HELPER_FLAGS_3(mulg, TCG_CALL_NO_RWG, i64, env, i64, i64)
DEF_HELPER_FLAGS_3(divg, TCG_CALL_NO_RWG, i64, env, i64, i64)
DEF_HELPER_FLAGS_2(sqrtg, TCG_CALL_NO_RWG, i64, env, i64)

DEF_HELPER_FLAGS_1(s_to_memory, TCG_CALL_NO_RWG_SE, i32, i64)
DEF_HELPER_FLAGS_1(memory_to_s, TCG_CALL_NO_RWG_SE, i64, i32)
DEF_HELPER_FLAGS_3(adds, TCG_CALL_NO_RWG, i64, env, i64, i64)
DEF_HELPER_FLAGS_3(subs, TCG_CALL_NO_RWG, i64, env, i64, i64)
DEF_HELPER_FLAGS_3(muls, TCG_CALL_NO_RWG, i64, env, i64, i64)
DEF_HELPER_FLAGS_3(divs, TCG_CALL_NO_RWG, i64, env, i64, i64)
DEF_HELPER_FLAGS_2(sqrts, TCG_CALL_NO_RWG, i64, env, i64)

DEF_HELPER_FLAGS_3(addt, TCG_CALL_NO_RWG, i64, env, i64, i64)
DEF_HELPER_FLAGS_3(subt, TCG_CALL_NO_RWG, i64, env, i64, i64)
DEF_HELPER_FLAGS_3(mult, TCG_CALL_NO_RWG, i64, env, i64, i64)
DEF_HELPER_FLAGS_3(divt, TCG_CALL_NO_RWG, i64, env, i64, i64)
DEF_HELPER_FLAGS_2(sqrtt, TCG_CALL_NO_RWG, i64, env, i64)

DEF_HELPER_FLAGS_3(cmptun, TCG_CALL_NO_RWG, i64, env, i64, i64)
DEF_HELPER_FLAGS_3(cmpteq, TCG_CALL_NO_RWG, i64, env, i64, i64)
DEF_HELPER_FLAGS_3(cmptle, TCG_CALL_NO_RWG, i64, env, i64, i64)
DEF_HELPER_FLAGS_3(cmptlt, TCG_CALL_NO_RWG, i64, env, i64, i64)
DEF_HELPER_FLAGS_3(cmpgeq, TCG_CALL_NO_RWG, i64, env, i64, i64)
DEF_HELPER_FLAGS_3(cmpgle, TCG_CALL_NO_RWG, i64, env, i64, i64)
DEF_HELPER_FLAGS_3(cmpglt, TCG_CALL_NO_RWG, i64, env, i64, i64)

DEF_HELPER_FLAGS_2(cvtts, TCG_CALL_NO_RWG, i64, env, i64)
DEF_HELPER_FLAGS_2(cvtst, TCG_CALL_NO_RWG, i64, env, i64)
DEF_HELPER_FLAGS_2(cvtqs, TCG_CALL_NO_RWG, i64, env, i64)
DEF_HELPER_FLAGS_2(cvtqt, TCG_CALL_NO_RWG, i64, env, i64)
DEF_HELPER_FLAGS_2(cvtqf, TCG_CALL_NO_RWG, i64, env, i64)
DEF_HELPER_FLAGS_2(cvtgf, TCG_CALL_NO_RWG, i64, env, i64)
DEF_HELPER_FLAGS_2(cvtgq, TCG_CALL_NO_RWG, i64, env, i64)
DEF_HELPER_FLAGS_2(cvtqg, TCG_CALL_NO_RWG, i64, env, i64)

DEF_HELPER_FLAGS_2(cvttq, TCG_CALL_NO_RWG, i64, env, i64)
DEF_HELPER_FLAGS_2(cvttq_c, TCG_CALL_NO_RWG, i64, env, i64)
DEF_HELPER_FLAGS_2(cvttq_svic, TCG_CALL_NO_RWG, i64, env, i64)

DEF_HELPER_FLAGS_2(setroundmode, TCG_CALL_NO_RWG, void, env, i32)
DEF_HELPER_FLAGS_2(setflushzero, TCG_CALL_NO_RWG, void, env, i32)
DEF_HELPER_FLAGS_1(fp_exc_clear, TCG_CALL_NO_RWG, void, env)
DEF_HELPER_FLAGS_1(fp_exc_get, TCG_CALL_NO_RWG_SE, i32, env)
DEF_HELPER_FLAGS_3(fp_exc_raise, TCG_CALL_NO_WG, void, env, i32, i32)
DEF_HELPER_FLAGS_3(fp_exc_raise_s, TCG_CALL_NO_WG, void, env, i32, i32)

DEF_HELPER_FLAGS_2(ieee_input, TCG_CALL_NO_WG, void, env, i64)
DEF_HELPER_FLAGS_2(ieee_input_cmp, TCG_CALL_NO_WG, void, env, i64)

#if !defined (CONFIG_USER_ONLY)
DEF_HELPER_2(hw_ret, void, env, i64)
DEF_HELPER_3(call_pal, void, env, i64, i64)

DEF_HELPER_1(ldl_phys, i64, i64)
DEF_HELPER_1(ldq_phys, i64, i64)
DEF_HELPER_2(ldl_l_phys, i64, env, i64)
DEF_HELPER_2(ldq_l_phys, i64, env, i64)
DEF_HELPER_2(stl_phys, void, i64, i64)
DEF_HELPER_2(stq_phys, void, i64, i64)
DEF_HELPER_3(stl_c_phys, i64, env, i64, i64)
DEF_HELPER_3(stq_c_phys, i64, env, i64, i64)

DEF_HELPER_FLAGS_1(tbia, TCG_CALL_NO_RWG, void, env)
DEF_HELPER_FLAGS_2(tbis, TCG_CALL_NO_RWG, void, env, i64)
DEF_HELPER_FLAGS_1(tb_flush, TCG_CALL_NO_RWG, void, env)

DEF_HELPER_1(halt, void, i64)

DEF_HELPER_FLAGS_0(get_vmtime, TCG_CALL_NO_RWG, i64)
DEF_HELPER_FLAGS_0(get_walltime, TCG_CALL_NO_RWG, i64)
DEF_HELPER_FLAGS_2(set_alarm, TCG_CALL_NO_RWG, void, env, i64)
#endif

#include "exec/def-helper.h"
