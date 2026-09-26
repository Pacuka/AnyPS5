#include "../include/Pthread.hpp"
#include "prx/libc/include/General.hpp"
#include <chrono>
#include <new>
#include <stdexcept>

namespace PthreadSync {

namespace {

bool IsStaticInitializer(PthreadMutexPrivate* value) {
    return value == nullptr || reinterpret_cast<std::uintptr_t>(value) == 1;
}

bool IsValidType(int type) {
    return type >= static_cast<int>(MutexType::ErrorCheck) && type <= static_cast<int>(MutexType::Adaptive);
}

}

Deadline DeadlineFromAbsolute_nid_no_patch(const KernelTimespec* abstime, int clockId) {
    if (!abstime || abstime->tv_nsec < 0 || abstime->tv_nsec >= 1000000000) throw std::invalid_argument("invalid absolute timeout");
    const auto target = std::chrono::seconds(abstime->tv_sec) + std::chrono::nanoseconds(abstime->tv_nsec);
    std::chrono::nanoseconds now;
    if (clockId == kClockMonotonic)
        now = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch());
    else
        now = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::system_clock::now().time_since_epoch());
    return std::chrono::steady_clock::now() + std::chrono::duration_cast<std::chrono::steady_clock::duration>(target - now);
}

Deadline DeadlineFromRelative_nid_no_patch(std::uint64_t microseconds) {
    return std::chrono::steady_clock::now() + std::chrono::microseconds(microseconds);
}

bool IsValidMutexType_nid_no_patch(int type) {
    return IsValidType(type);
}

PthreadMutexPrivate* MutexResolve_nid_no_patch(PthreadMutex* mutex, MutexType defaultType) {
    if (!mutex) return nullptr;
    auto& slot = *reinterpret_cast<std::atomic<PthreadMutexPrivate*>*>(mutex);
    auto* current = slot.load(std::memory_order_acquire);
    if (!IsStaticInitializer(current)) return current;
    auto* created = new PthreadMutexPrivate();
    created->_type = reinterpret_cast<std::uintptr_t>(current) == 1 ? MutexType::Adaptive : defaultType;
    if (slot.compare_exchange_strong(current, created, std::memory_order_acq_rel)) return created;
    delete created;
    return current;
}

int MutexInit_nid_no_patch(PthreadMutex* mutex, const PthreadMutexattr* attr, MutexType defaultType) {
    if (!mutex) return kEINVAL;
    auto* created = new (std::nothrow) PthreadMutexPrivate();
    if (!created) return kENOMEM;
    created->_type = (attr && *attr) ? (*attr)->type : defaultType;
    *mutex = created;
    return 0;
}

int MutexDestroy_nid_no_patch(PthreadMutex* mutex) {
    if (!mutex) return kEINVAL;
    if (IsStaticInitializer(*mutex)) {
        *mutex = nullptr;
        return 0;
    }
    if ((*mutex)->_owner.load(std::memory_order_acquire) != std::thread::id{}) return kEBUSY;
    delete *mutex;
    *mutex = nullptr;
    return 0;
}

int MutexLock_nid_no_patch(PthreadMutex* mutex, MutexType defaultType, const Deadline* deadline) {
    auto* m = MutexResolve_nid_no_patch(mutex, defaultType);
    if (!m) return kEINVAL;
    const auto self = std::this_thread::get_id();
    if (m->_owner.load(std::memory_order_acquire) == self) {
        if (m->_type != MutexType::Recursive) return kEDEADLK;
        ++m->_count;
        return 0;
    }
    if (deadline) {
        if (!m->_mtx.try_lock_until(*deadline)) return kETIMEDOUT;
    } else {
        m->_mtx.lock();
    }
    m->_owner.store(self, std::memory_order_release);
    m->_count = 1;
    return 0;
}

int MutexTrylock_nid_no_patch(PthreadMutex* mutex, MutexType defaultType) {
    auto* m = MutexResolve_nid_no_patch(mutex, defaultType);
    if (!m) return kEINVAL;
    const auto self = std::this_thread::get_id();
    if (m->_owner.load(std::memory_order_acquire) == self) {
        if (m->_type != MutexType::Recursive) return kEBUSY;
        ++m->_count;
        return 0;
    }
    if (!m->_mtx.try_lock()) return kEBUSY;
    m->_owner.store(self, std::memory_order_release);
    m->_count = 1;
    return 0;
}

int MutexUnlock_nid_no_patch(PthreadMutex* mutex) {
    if (!mutex || IsStaticInitializer(*mutex)) return kEPERM;
    auto* m = *mutex;
    if (m->_owner.load(std::memory_order_acquire) != std::this_thread::get_id()) {
        if (m->_type == MutexType::ErrorCheck || m->_type == MutexType::Recursive) return kEPERM;
        m->_count = 1;
    }
    if (--m->_count > 0) return 0;
    m->_count = 0;
    m->_owner.store(std::thread::id{}, std::memory_order_release);
    m->_mtx.unlock();
    return 0;
}

}

using namespace PthreadSync;

extern "C" {

int APS5_VABI scePthreadMutexattrInit(PthreadMutexattr* attr) {
    if (!attr) return ToSce(kEINVAL);
    auto* p = new (std::nothrow) PthreadMutexattrPrivate{};
    if (!p) return ToSce(kENOMEM);
    *attr = p;
    return 0;
}

int APS5_VABI scePthreadMutexattrDestroy(PthreadMutexattr* attr) {
    if (!attr || !*attr) return ToSce(kEINVAL);
    delete *attr;
    *attr = nullptr;
    return 0;
}

int APS5_VABI scePthreadMutexattrSettype(PthreadMutexattr* attr, int type) {
    if (!attr || !*attr || !IsValidMutexType_nid_no_patch(type)) return ToSce(kEINVAL);
    (*attr)->type = static_cast<MutexType>(type);
    return 0;
}

int APS5_VABI scePthreadMutexattrGettype(const PthreadMutexattr* attr, int* type) {
    if (!attr || !*attr || !type) return ToSce(kEINVAL);
    *type = static_cast<int>((*attr)->type);
    return 0;
}

int APS5_VABI scePthreadMutexattrSetprotocol(PthreadMutexattr* attr, int protocol) {
    if (!attr || !*attr || protocol < 0 || protocol > 2) return ToSce(kEINVAL);
    (*attr)->protocol = protocol;
    return 0;
}

int APS5_VABI scePthreadMutexInit(PthreadMutex* mutex, const PthreadMutexattr* attr, const char*) {
    return ToSce(MutexInit_nid_no_patch(mutex, attr, MutexType::ErrorCheck));
}

int APS5_VABI scePthreadMutexDestroy(PthreadMutex* mutex) {
    return ToSce(MutexDestroy_nid_no_patch(mutex));
}

int APS5_VABI scePthreadMutexLock(PthreadMutex* mutex) {
    return ToSce(MutexLock_nid_no_patch(mutex, MutexType::ErrorCheck, nullptr));
}

int APS5_VABI scePthreadMutexTimedlock(PthreadMutex* mutex, KernelUseconds usec) {
    const auto deadline = DeadlineFromRelative_nid_no_patch(usec);
    return ToSce(MutexLock_nid_no_patch(mutex, MutexType::ErrorCheck, &deadline));
}

int APS5_VABI scePthreadMutexTrylock(PthreadMutex* mutex) {
    return ToSce(MutexTrylock_nid_no_patch(mutex, MutexType::ErrorCheck));
}

int APS5_VABI scePthreadMutexUnlock(PthreadMutex* mutex) {
    return ToSce(MutexUnlock_nid_no_patch(mutex));
}

}
