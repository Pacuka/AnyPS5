#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"
#include "../include/Pthread.hpp"

using namespace PthreadSync;

extern "C" {

int APS5_VABI scePthreadMutexattrInit(PthreadMutexattr* attr);
int APS5_VABI scePthreadMutexattrDestroy(PthreadMutexattr* attr);
int APS5_VABI scePthreadMutexattrSetprotocol(PthreadMutexattr* attr, int protocol);
int APS5_VABI scePthreadMutexattrSettype(PthreadMutexattr* attr, int type);

int APS5_VABI pthread_mutex_destroy_nid_postfix(PthreadMutex* mutex) {
    return MutexDestroy_nid_no_patch(mutex);
}

int APS5_VABI pthread_mutex_init_nid_postfix(PthreadMutex* mutex, const PthreadMutexattr* attr) {
    return MutexInit_nid_no_patch(mutex, attr, MutexType::ErrorCheck);
}

int APS5_VABI pthread_mutex_lock_nid_postfix(PthreadMutex* mutex) {
    return MutexLock_nid_no_patch(mutex, MutexType::ErrorCheck, nullptr);
}

int APS5_VABI pthread_mutex_timedlock_nid_postfix(PthreadMutex* mutex, const KernelTimespec* abstime) {
    const auto deadline = DeadlineFromAbsolute_nid_no_patch(abstime, kClockRealtime);
    return MutexLock_nid_no_patch(mutex, MutexType::ErrorCheck, &deadline);
}

int APS5_VABI pthread_mutex_trylock_nid_postfix(PthreadMutex* mutex) {
    return MutexTrylock_nid_no_patch(mutex, MutexType::ErrorCheck);
}

int APS5_VABI pthread_mutex_unlock_nid_postfix(PthreadMutex* mutex) {
    return MutexUnlock_nid_no_patch(mutex);
}

int APS5_VABI pthread_mutexattr_destroy_nid_postfix(PthreadMutexattr* attr) {
    return FromSce_nid_no_patch(scePthreadMutexattrDestroy(attr));
}

int APS5_VABI pthread_mutexattr_init_nid_postfix(PthreadMutexattr* attr) {
    return FromSce_nid_no_patch(scePthreadMutexattrInit(attr));
}

int APS5_VABI pthread_mutexattr_setprotocol_nid_postfix(PthreadMutexattr* attr, int protocol) {
    return FromSce_nid_no_patch(scePthreadMutexattrSetprotocol(attr, protocol));
}

int APS5_VABI pthread_mutexattr_settype_nid_postfix(PthreadMutexattr* attr, int type) {
    return FromSce_nid_no_patch(scePthreadMutexattrSettype(attr, type));
}


}
