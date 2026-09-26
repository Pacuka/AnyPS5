#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"
#include "../include/Pthread.hpp"

using namespace PthreadSync;

extern "C" {

int APS5_VABI scePthreadCreate(Pthread* thread, const PthreadAttr* attr, PthreadEntry entry, void* arg, const char* name);
int APS5_VABI scePthreadJoin(Pthread thread, void** retval);
int APS5_VABI scePthreadDetach(Pthread thread);
void APS5_VABI scePthreadExit(void* retval);
Pthread APS5_VABI scePthreadSelf();
void APS5_VABI scePthreadYield();
int APS5_VABI scePthreadRename(Pthread thread, const char* name);
int APS5_VABI scePthreadSetprio(Pthread thread, int prio);
int APS5_VABI scePthreadGetprio(Pthread thread, int* prio);
int APS5_VABI scePthreadSetcancelstate(int state, int* old_state);

int APS5_VABI pthread_create_nid_postfix(Pthread* thread, const PthreadAttr* attr, PthreadEntry entry, void* arg) {
    return FromSce_nid_no_patch(scePthreadCreate(thread, attr, entry, arg, nullptr));
}

int APS5_VABI pthread_create_name_np_nid_postfix(Pthread* thread, const PthreadAttr* attr, PthreadEntry entry, void* arg, const char* name) {
    return FromSce_nid_no_patch(scePthreadCreate(thread, attr, entry, arg, name));
}

int APS5_VABI pthread_detach_nid_postfix(Pthread thread) {
    return FromSce_nid_no_patch(scePthreadDetach(thread));
}

void APS5_VABI pthread_exit_nid_postfix(void* value) {
    scePthreadExit(value);
}

int APS5_VABI pthread_equal_nid_postfix(Pthread thread1, Pthread thread2) {
    return thread1 == thread2 ? 1 : 0;
}

int APS5_VABI pthread_getschedparam_nid_postfix(Pthread thread, int* policy, KernelSchedParam* param) {
    if (!thread || !policy || !param) return kEINVAL;
    *policy = 1;
    return FromSce_nid_no_patch(scePthreadGetprio(thread, &param->sched_priority));
}

int APS5_VABI pthread_join_nid_postfix(Pthread thread, void** value) {
    return FromSce_nid_no_patch(scePthreadJoin(thread, value));
}

int APS5_VABI pthread_rename_np_nid_postfix(Pthread thread, const char* name) {
    return FromSce_nid_no_patch(scePthreadRename(thread, name));
}

Pthread APS5_VABI pthread_self_nid_postfix(void) {
    return scePthreadSelf();
}

int APS5_VABI pthread_setcancelstate_nid_postfix(int state, int* old_state) {
    return FromSce_nid_no_patch(scePthreadSetcancelstate(state, old_state));
}

int APS5_VABI pthread_setprio_nid_postfix(Pthread thread, int prio) {
    return FromSce_nid_no_patch(scePthreadSetprio(thread, prio));
}

int APS5_VABI pthread_setschedparam_nid_postfix(Pthread thread, int policy, const KernelSchedParam* param) {
    (void)policy;
    if (!param) return kEINVAL;
    return FromSce_nid_no_patch(scePthreadSetprio(thread, param->sched_priority));
}

void APS5_VABI pthread_yield_nid_postfix(void) {
    scePthreadYield();
}

}
