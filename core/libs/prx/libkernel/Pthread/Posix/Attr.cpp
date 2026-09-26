#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"
#include "../include/Pthread.hpp"

using namespace PthreadSync;

extern "C" {

int APS5_VABI scePthreadAttrInit(PthreadAttr* attr);
int APS5_VABI scePthreadAttrDestroy(PthreadAttr* attr);
int APS5_VABI scePthreadAttrGet(Pthread thread, PthreadAttr* attr);
int APS5_VABI scePthreadAttrGetdetachstate(const PthreadAttr* attr, int* state);
int APS5_VABI scePthreadAttrSetdetachstate(PthreadAttr* attr, int state);
int APS5_VABI scePthreadAttrGetguardsize(const PthreadAttr* attr, size_t* guard_size);
int APS5_VABI scePthreadAttrSetguardsize(PthreadAttr* attr, size_t guard_size);
int APS5_VABI scePthreadAttrGetschedparam(const PthreadAttr* attr, KernelSchedParam* param);
int APS5_VABI scePthreadAttrSetschedparam(PthreadAttr* attr, const KernelSchedParam* param);
int APS5_VABI scePthreadAttrGetschedpolicy(const PthreadAttr* attr, int* policy);
int APS5_VABI scePthreadAttrSetschedpolicy(PthreadAttr* attr, int policy);
int APS5_VABI scePthreadAttrGetstack(const PthreadAttr* attr, void** stack_addr, size_t* stack_size);
int APS5_VABI scePthreadAttrSetstack(PthreadAttr* attr, void* addr, size_t size);
int APS5_VABI scePthreadAttrGetstacksize(const PthreadAttr* attr, size_t* stack_size);
int APS5_VABI scePthreadAttrSetstacksize(PthreadAttr* attr, size_t stack_size);
int APS5_VABI scePthreadAttrSetinheritsched(PthreadAttr* attr, int inherit_sched);
int APS5_VABI scePthreadAttrSetsolosched(PthreadAttr* attr, int solosched);

int APS5_VABI pthread_attr_destroy_nid_postfix(PthreadAttr* attr) {
    return FromSce_nid_no_patch(scePthreadAttrDestroy(attr));
}

int APS5_VABI pthread_attr_get_np_nid_postfix(Pthread thread, PthreadAttr* attr) {
    return FromSce_nid_no_patch(scePthreadAttrGet(thread, attr));
}

int APS5_VABI pthread_attr_getdetachstate_nid_postfix(const PthreadAttr* attr, int* state) {
    return FromSce_nid_no_patch(scePthreadAttrGetdetachstate(attr, state));
}

int APS5_VABI pthread_attr_getguardsize_nid_postfix(const PthreadAttr* attr, size_t* guard_size) {
    return FromSce_nid_no_patch(scePthreadAttrGetguardsize(attr, guard_size));
}

int APS5_VABI pthread_attr_getschedparam_nid_postfix(const PthreadAttr* attr, KernelSchedParam* param) {
    return FromSce_nid_no_patch(scePthreadAttrGetschedparam(attr, param));
}

int APS5_VABI pthread_attr_getschedpolicy_nid_postfix(const PthreadAttr* attr, int* policy) {
    return FromSce_nid_no_patch(scePthreadAttrGetschedpolicy(attr, policy));
}

int APS5_VABI pthread_attr_getstack_nid_postfix(const PthreadAttr* __restrict attr, void** __restrict stack_addr, size_t* __restrict stack_size) {
    return FromSce_nid_no_patch(scePthreadAttrGetstack(attr, stack_addr, stack_size));
}

int APS5_VABI pthread_attr_getstacksize_nid_postfix(const PthreadAttr* attr, size_t* stack_size) {
    return FromSce_nid_no_patch(scePthreadAttrGetstacksize(attr, stack_size));
}

int APS5_VABI pthread_attr_init_nid_postfix(PthreadAttr* attr) {
    return FromSce_nid_no_patch(scePthreadAttrInit(attr));
}

int APS5_VABI pthread_attr_setdetachstate_nid_postfix(PthreadAttr* attr, int state) {
    return FromSce_nid_no_patch(scePthreadAttrSetdetachstate(attr, state));
}

int APS5_VABI pthread_attr_setguardsize_nid_postfix(PthreadAttr* attr, size_t guard_size) {
    return FromSce_nid_no_patch(scePthreadAttrSetguardsize(attr, guard_size));
}

int APS5_VABI pthread_attr_setinheritsched_nid_postfix(PthreadAttr* attr, int inherit_sched) {
    return FromSce_nid_no_patch(scePthreadAttrSetinheritsched(attr, inherit_sched));
}

int APS5_VABI pthread_attr_setschedparam_nid_postfix(PthreadAttr* attr, const KernelSchedParam* param) {
    return FromSce_nid_no_patch(scePthreadAttrSetschedparam(attr, param));
}

int APS5_VABI pthread_attr_setschedpolicy_nid_postfix(PthreadAttr* attr, int policy) {
    return FromSce_nid_no_patch(scePthreadAttrSetschedpolicy(attr, policy));
}

int APS5_VABI pthread_attr_setsolosched_np_nid_postfix(PthreadAttr* attr, int solosched) {
    return FromSce_nid_no_patch(scePthreadAttrSetsolosched(attr, solosched));
}

int APS5_VABI pthread_attr_setstack_nid_postfix(PthreadAttr* attr, void* addr, size_t size) {
    return FromSce_nid_no_patch(scePthreadAttrSetstack(attr, addr, size));
}

int APS5_VABI pthread_attr_setstacksize_nid_postfix(PthreadAttr* attr, size_t stack_size) {
    return FromSce_nid_no_patch(scePthreadAttrSetstacksize(attr, stack_size));
}


}
