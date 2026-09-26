#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"
#include "../include/Pthread.hpp"

using namespace PthreadSync;

extern "C" {

int APS5_VABI pthread_rwlock_destroy_nid_postfix(PthreadRwlock* rwlock) {
    return RwlockDestroy_nid_no_patch(rwlock);
}

int APS5_VABI pthread_rwlock_init_nid_postfix(PthreadRwlock* rwlock, const PthreadRwlockattr* attr) {
    (void)attr;
    return RwlockInit_nid_no_patch(rwlock);
}

int APS5_VABI pthread_rwlock_rdlock_nid_postfix(PthreadRwlock* rwlock) {
    return RwlockRead_nid_no_patch(rwlock, false, nullptr);
}

int APS5_VABI pthread_rwlock_tryrdlock_nid_postfix(PthreadRwlock* rwlock) {
    return RwlockRead_nid_no_patch(rwlock, true, nullptr);
}

int APS5_VABI pthread_rwlock_wrlock_nid_postfix(PthreadRwlock* rwlock) {
    return RwlockWrite_nid_no_patch(rwlock, false, nullptr);
}

int APS5_VABI pthread_rwlock_trywrlock_nid_postfix(PthreadRwlock* rwlock) {
    return RwlockWrite_nid_no_patch(rwlock, true, nullptr);
}

int APS5_VABI pthread_rwlock_unlock_nid_postfix(PthreadRwlock* rwlock) {
    return RwlockUnlock_nid_no_patch(rwlock);
}


}
