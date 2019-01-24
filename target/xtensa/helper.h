DEF_HELPER_2(exception, noreturn, env, i32)
DEF_HELPER_3(exception_cause, noreturn, env, i32, i32)
DEF_HELPER_4(exception_cause_vaddr, noreturn, env, i32, i32, i32)
DEF_HELPER_3(debug_exception, noreturn, env, i32, i32)

DEF_HELPER_2(wsr_windowbase, void, env, i32)
DEF_HELPER_4(entry, void, env, i32, i32, i32)
DEF_HELPER_2(test_ill_retw, void, env, i32)
DEF_HELPER_2(test_underflow_retw, void, env, i32)
DEF_HELPER_2(retw, i32, env, i32)
DEF_HELPER_2(rotw, void, env, i32)
DEF_HELPER_3(window_check, noreturn, env, i32, i32)
DEF_HELPER_1(restore_owb, void, env)
DEF_HELPER_2(movsp, void, env, i32)
#ifndef CONFIG_USER_ONLY
DEF_HELPER_1(simcall, void, env)
#endif

#ifndef CONFIG_USER_ONLY
DEF_HELPER_3(waiti, void, env, i32, i32)
DEF_HELPER_1(update_ccount, void, env)
DEF_HELPER_2(wsr_ccount, void, env, i32)
DEF_HELPER_2(update_ccompare, void, env, i32)
DEF_HELPER_1(check_interrupts, void, env)
DEF_HELPER_2(intset, void, env, i32)
DEF_HELPER_2(intclear, void, env, i32)
DEF_HELPER_3(check_atomctl, void, env, i32, i32)
DEF_HELPER_2(wsr_memctl, void, env, i32)

DEF_HELPER_2(itlb_hit_test, void, env, i32)
DEF_HELPER_2(wsr_rasid, void, env, i32)
DEF_HELPER_FLAGS_3(rtlb0, TCG_CALL_NO_RWG_SE, i32, env, i32, i32)
DEF_HELPER_FLAGS_3(rtlb1, TCG_CALL_NO_RWG_SE, i32, env, i32, i32)
DEF_HELPER_3(itlb, void, env, i32, i32)
DEF_HELPER_3(ptlb, i32, env, i32, i32)
DEF_HELPER_4(wtlb, void, env, i32, i32, i32)

DEF_HELPER_2(wsr_ibreakenable, void, env, i32)
DEF_HELPER_3(wsr_ibreaka, void, env, i32, i32)
DEF_HELPER_3(wsr_dbreaka, void, env, i32, i32)
DEF_HELPER_3(wsr_dbreakc, void, env, i32, i32)
#endif

DEF_HELPER_2(wur_fcr, void, env, i32)
DEF_HELPER_FLAGS_1(abs_s, TCG_CALL_NO_RWG_SE, f32, f32)
DEF_HELPER_FLAGS_1(neg_s, TCG_CALL_NO_RWG_SE, f32, f32)
DEF_HELPER_3(add_s, f32, env, f32, f32)
DEF_HELPER_3(sub_s, f32, env, f32, f32)
DEF_HELPER_3(mul_s, f32, env, f32, f32)
DEF_HELPER_4(madd_s, f32, env, f32, f32, f32)
DEF_HELPER_4(msub_s, f32, env, f32, f32, f32)
DEF_HELPER_FLAGS_3(ftoi, TCG_CALL_NO_RWG_SE, i32, f32, i32, i32)
DEF_HELPER_FLAGS_3(ftoui, TCG_CALL_NO_RWG_SE, i32, f32, i32, i32)
DEF_HELPER_3(itof, f32, env, i32, i32)
DEF_HELPER_3(uitof, f32, env, i32, i32)

DEF_HELPER_4(un_s, void, env, i32, f32, f32)
DEF_HELPER_4(oeq_s, void, env, i32, f32, f32)
DEF_HELPER_4(ueq_s, void, env, i32, f32, f32)
DEF_HELPER_4(olt_s, void, env, i32, f32, f32)
DEF_HELPER_4(ult_s, void, env, i32, f32, f32)
DEF_HELPER_4(ole_s, void, env, i32, f32, f32)
DEF_HELPER_4(ule_s, void, env, i32, f32, f32)

DEF_HELPER_2(rer, i32, env, i32)
DEF_HELPER_3(wer, void, env, i32, i32)
