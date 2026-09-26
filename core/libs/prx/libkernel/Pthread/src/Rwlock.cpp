#include "../include/Pthread.hpp"
#include "prx/libc/include/General.hpp"
#include <new>

namespace PthreadSync {

namespace {

PthreadRwlockPrivate* Resolve(PthreadRwlock* rwlock) {
    if (!rwlock) return nullptr;
    auto& slot = *reinterpret_cast<std::atomic<PthreadRwlockPrivate*>*>(rwlock);
    auto* current = slot.load(std::memory_order_acquire);
    if (current) return current;
    auto* created = new PthreadRwlockPrivate();
    if (slot.compare_exchange_strong(current, created, std::memory_order_acq_rel)) return created;
    delete created;
    return current;
}

}

int RwlockInit_nid_no_patch(PthreadRwlock* rwlock) {
    if (!rwlock) return kEINVAL;
    auto* created = new (std::nothrow) PthreadRwlockPrivate();
    if (!created) return kENOMEM;
    *rwlock = created;
    return 0;
}

int RwlockDestroy_nid_no_patch(PthreadRwlock* rwlock) {
    if (!rwlock) return kEINVAL;
    delete *rwlock;
    *rwlock = nullptr;
    return 0;
}

int RwlockRead_nid_no_patch(PthreadRwlock* rwlock, bool tryOnly, const Deadline* deadline) {
    auto* lock = Resolve(rwlock);
    if (!lock) return kEINVAL;
    if (lock->_writer.load(std::memory_order_acquire) == std::this_thread::get_id()) return kEDEADLK;
    if (tryOnly) return lock->_lock.try_lock_shared() ? 0 : kEBUSY;
    if (deadline) return lock->_lock.try_lock_shared_until(*deadline) ? 0 : kETIMEDOUT;
    lock->_lock.lock_shared();
    return 0;
}

int RwlockWrite_nid_no_patch(PthreadRwlock* rwlock, bool tryOnly, const Deadline* deadline) {
    auto* lock = Resolve(rwlock);
    if (!lock) return kEINVAL;
    if (lock->_writer.load(std::memory_order_acquire) == std::this_thread::get_id()) return kEDEADLK;
    if (tryOnly) {
        if (!lock->_lock.try_lock()) return kEBUSY;
    } else if (deadline) {
        if (!lock->_lock.try_lock_until(*deadline)) return kETIMEDOUT;
    } else {
        lock->_lock.lock();
    }
    lock->_writer.store(std::this_thread::get_id(), std::memory_order_release);
    return 0;
}

int RwlockUnlock_nid_no_patch(PthreadRwlock* rwlock) {
    if (!rwlock || !*rwlock) return kEINVAL;
    auto* lock = *rwlock;
    if (lock->_writer.load(std::memory_order_acquire) == std::this_thread::get_id()) {
        lock->_writer.store(std::thread::id{}, std::memory_order_release);
        lock->_lock.unlock();
        return 0;
    }
    lock->_lock.unlock_shared();
    return 0;
}

}

using namespace PthreadSync;

extern "C" {

int APS5_VABI scePthreadRwlockInit(PthreadRwlock* rwlock, const PthreadRwlockattr*, const char*) {
    return ToSce(RwlockInit_nid_no_patch(rwlock));
}

int APS5_VABI scePthreadRwlockDestroy(PthreadRwlock* rwlock) {
    return ToSce(RwlockDestroy_nid_no_patch(rwlock));
}

int APS5_VABI scePthreadRwlockRdlock(PthreadRwlock* rwlock) {
    return ToSce(RwlockRead_nid_no_patch(rwlock, false, nullptr));
}

int APS5_VABI scePthreadRwlockTryrdlock(PthreadRwlock* rwlock) {
    return ToSce(RwlockRead_nid_no_patch(rwlock, true, nullptr));
}

int APS5_VABI scePthreadRwlockWrlock(PthreadRwlock* rwlock) {
    return ToSce(RwlockWrite_nid_no_patch(rwlock, false, nullptr));
}

int APS5_VABI scePthreadRwlockTrywrlock(PthreadRwlock* rwlock) {
    return ToSce(RwlockWrite_nid_no_patch(rwlock, true, nullptr));
}

int APS5_VABI scePthreadRwlockUnlock(PthreadRwlock* rwlock) {
    return ToSce(RwlockUnlock_nid_no_patch(rwlock));
}

int APS5_VABI scePthreadRwlockattrInit(PthreadRwlockattr* attr) {
    if (!attr) return ToSce(kEINVAL);
    auto* p = new (std::nothrow) PthreadRwlockattrPrivate{};
    if (!p) return ToSce(kENOMEM);
    *attr = p;
    return 0;
}

int APS5_VABI scePthreadRwlockattrDestroy(PthreadRwlockattr* attr) {
    if (!attr || !*attr) return ToSce(kEINVAL);
    delete *attr;
    *attr = nullptr;
    return 0;
}

int APS5_VABI scePthreadRwlockattrSettype(PthreadRwlockattr* attr, int type) {
    if (!attr || !*attr) return ToSce(kEINVAL);
    (*attr)->type = type;
    return 0;
}


}
