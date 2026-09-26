#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"
#include "../include/Pthread.hpp"

using namespace PthreadSync;

extern "C" {

int APS5_VABI scePthreadCondattrInit(PthreadCondattr* attr);
int APS5_VABI scePthreadCondattrDestroy(PthreadCondattr* attr);

int APS5_VABI pthread_cond_broadcast_nid_postfix(PthreadCond* cond) {
    return CondSignal_nid_no_patch(cond, true);
}

int APS5_VABI pthread_cond_destroy_nid_postfix(PthreadCond* cond) {
    return CondDestroy_nid_no_patch(cond);
}

int APS5_VABI pthread_cond_init_nid_postfix(PthreadCond* cond, const PthreadCondattr* attr) {
    return CondInit_nid_no_patch(cond, attr);
}

int APS5_VABI pthread_cond_signal_nid_postfix(PthreadCond* cond) {
    return CondSignal_nid_no_patch(cond, false);
}

int APS5_VABI pthread_cond_timedwait_nid_postfix(PthreadCond* cond, PthreadMutex* mutex, const KernelTimespec* abstime) {
    auto* resolved = CondResolve_nid_no_patch(cond);
    if (!resolved) return kEINVAL;
    const auto deadline = DeadlineFromAbsolute_nid_no_patch(abstime, resolved->_clockid);
    return CondWait_nid_no_patch(cond, mutex, MutexType::ErrorCheck, &deadline);
}

int APS5_VABI pthread_cond_wait_nid_postfix(PthreadCond* cond, PthreadMutex* mutex) {
    return CondWait_nid_no_patch(cond, mutex, MutexType::ErrorCheck, nullptr);
}

int APS5_VABI pthread_condattr_destroy_nid_postfix(PthreadCondattr* attr) {
    return FromSce_nid_no_patch(scePthreadCondattrDestroy(attr));
}

int APS5_VABI pthread_condattr_init_nid_postfix(PthreadCondattr* attr) {
    return FromSce_nid_no_patch(scePthreadCondattrInit(attr));
}

int APS5_VABI pthread_condattr_setclock_nid_postfix(PthreadCondattr* attr, KernelClockid clock_id) {
    if (!attr || !*attr || (clock_id != kClockRealtime && clock_id != kClockMonotonic)) return kEINVAL;
    (*attr)->_clockid = clock_id;
    return 0;
}


}
