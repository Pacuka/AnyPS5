#include "../include/Pthread.hpp"
#include "prx/libc/include/General.hpp"
#include <new>
#include <stdexcept>

#ifdef _WIN32
#include <windows.h>
#include <limits>
#endif

static constexpr std::size_t DEFAULT_STACK_SIZE = 1u << 20;
static constexpr std::size_t MINIMUM_STACK_SIZE = 16384;
static constexpr int DETACH_JOINABLE = 0;
static constexpr int DETACH_DETACHED = 1;

using namespace PthreadSync;

namespace {

bool IsValid(const PthreadAttr* attr) {
    return attr && *attr;
}

bool IsValidStackSize(std::size_t stacksize) {
    if (stacksize < MINIMUM_STACK_SIZE) return false;
#ifdef _WIN32
    SYSTEM_INFO system{};
    GetSystemInfo(&system);
    if (stacksize % system.dwPageSize != 0 || stacksize > std::numeric_limits<unsigned>::max()) return false;
#endif
    return true;
}

}

extern "C" {

int APS5_VABI scePthreadAttrInit(PthreadAttr* attr) {
    if (!attr) return ToSce(kEINVAL);
    auto* p = new (std::nothrow) PthreadAttrPrivate{};
    if (!p) return ToSce(kENOMEM);
    p->_stacksize = DEFAULT_STACK_SIZE;
    *attr = p;
    return 0;
}

int APS5_VABI scePthreadAttrDestroy(PthreadAttr* attr) {
    if (!IsValid(attr)) return ToSce(kEINVAL);
    delete *attr;
    *attr = nullptr;
    return 0;
}

int APS5_VABI scePthreadAttrSetdetachstate(PthreadAttr* attr, int detachstate) {
    if (!IsValid(attr) || (detachstate != DETACH_JOINABLE && detachstate != DETACH_DETACHED)) return ToSce(kEINVAL);
    (*attr)->_detachstate = detachstate;
    return 0;
}

int APS5_VABI scePthreadAttrGetdetachstate(const PthreadAttr* attr, int* state) {
    if (!IsValid(attr) || !state) return ToSce(kEINVAL);
    *state = (*attr)->_detachstate;
    return 0;
}

int APS5_VABI scePthreadAttrSetschedparam(PthreadAttr* attr, const KernelSchedParam* param) {
    if (!IsValid(attr) || !param) return ToSce(kEINVAL);
    (*attr)->_schedpriority = param->sched_priority;
    return 0;
}

int APS5_VABI scePthreadAttrGetschedparam(const PthreadAttr* attr, KernelSchedParam* param) {
    if (!IsValid(attr) || !param) return ToSce(kEINVAL);
    param->sched_priority = (*attr)->_schedpriority;
    return 0;
}

int APS5_VABI scePthreadAttrSetschedpolicy(PthreadAttr* attr, int policy) {
    if (!IsValid(attr)) return ToSce(kEINVAL);
    (*attr)->_schedpolicy = policy;
    return 0;
}

int APS5_VABI scePthreadAttrGetschedpolicy(const PthreadAttr* attr, int* policy) {
    if (!IsValid(attr) || !policy) return ToSce(kEINVAL);
    *policy = (*attr)->_schedpolicy;
    return 0;
}

int APS5_VABI scePthreadAttrSetinheritsched(PthreadAttr* attr, int inherit_sched) {
    if (!IsValid(attr)) return ToSce(kEINVAL);
    (*attr)->_inheritsched = inherit_sched;
    return 0;
}

int APS5_VABI scePthreadAttrGetinheritsched(const PthreadAttr* attr, int* inherit_sched) {
    if (!IsValid(attr) || !inherit_sched) return ToSce(kEINVAL);
    *inherit_sched = (*attr)->_inheritsched;
    return 0;
}

int APS5_VABI scePthreadAttrSetstacksize(PthreadAttr* attr, std::size_t stacksize) {
    if (!IsValid(attr) || !IsValidStackSize(stacksize)) return ToSce(kEINVAL);
    (*attr)->_stacksize = stacksize;
    return 0;
}

int APS5_VABI scePthreadAttrGetstacksize(const PthreadAttr* attr, size_t* stack_size) {
    if (!IsValid(attr) || !stack_size) return ToSce(kEINVAL);
    *stack_size = (*attr)->_stacksize;
    return 0;
}

int APS5_VABI scePthreadAttrSetstackaddr(PthreadAttr* attr, void* addr) {
    if (!IsValid(attr)) return ToSce(kEINVAL);
    (*attr)->stackAddress = addr;
    return 0;
}

int APS5_VABI scePthreadAttrGetstackaddr(const PthreadAttr* attr, void** stack_addr) {
    if (!IsValid(attr) || !stack_addr) return ToSce(kEINVAL);
    *stack_addr = (*attr)->stackAddress;
    return 0;
}

int APS5_VABI scePthreadAttrSetstack(PthreadAttr* attr, void* addr, size_t size) {
    if (!IsValid(attr) || !IsValidStackSize(size)) return ToSce(kEINVAL);
    (*attr)->stackAddress = addr;
    (*attr)->_stacksize = size;
    return 0;
}

int APS5_VABI scePthreadAttrGetstack(const PthreadAttr* attr, void** stackaddr, std::size_t* stacksize) {
    if (!IsValid(attr) || !stackaddr || !stacksize) return ToSce(kEINVAL);
    *stackaddr = (*attr)->stackAddress;
    *stacksize = (*attr)->_stacksize;
    return 0;
}

int APS5_VABI scePthreadAttrSetguardsize(PthreadAttr* attr, size_t guard_size) {
    if (!IsValid(attr)) return ToSce(kEINVAL);
    (*attr)->guardSize = guard_size;
    return 0;
}

int APS5_VABI scePthreadAttrGetguardsize(const PthreadAttr* attr, size_t* guard_size) {
    if (!IsValid(attr) || !guard_size) return ToSce(kEINVAL);
    *guard_size = (*attr)->guardSize;
    return 0;
}

int APS5_VABI scePthreadAttrSetaffinity(PthreadAttr* attr, KernelCpumask mask) {
    if (!IsValid(attr)) return ToSce(kEINVAL);
    (*attr)->affinity = mask;
    return 0;
}

int APS5_VABI scePthreadAttrGetaffinity(const PthreadAttr* attr, KernelCpumask* mask) {
    if (!IsValid(attr) || !mask) return ToSce(kEINVAL);
    *mask = (*attr)->affinity == 0 ? 0xFF : (*attr)->affinity;
    return 0;
}

int APS5_VABI scePthreadAttrSetsolosched(PthreadAttr* attr, int solosched) {
    if (!IsValid(attr)) return ToSce(kEINVAL);
    (*attr)->solosched = solosched;
    return 0;
}

int APS5_VABI scePthreadAttrGetsolosched(const PthreadAttr* attr, int* solosched) {
    if (!IsValid(attr) || !solosched) return ToSce(kEINVAL);
    *solosched = (*attr)->solosched;
    return 0;
}

int APS5_VABI scePthreadAttrGet(Pthread thread, PthreadAttr* attr) {
    if (!thread || !IsValid(attr)) return ToSce(kEINVAL);
    (*attr)->_stacksize = thread->stackSize;
    (*attr)->stackAddress = thread->stackAddress;
    (*attr)->_detachstate = thread->_detached ? DETACH_DETACHED : DETACH_JOINABLE;
    (*attr)->_schedpriority = thread->priority;
    (*attr)->affinity = thread->affinity;
    return 0;
}

}
