#include "../include/Pthread.hpp"
#include "prx/libc/include/General.hpp"
#include <chrono>
#include <new>
#include <stdexcept>

namespace PthreadSync {

namespace {

class MutexWaitAdapter {
public:
    explicit MutexWaitAdapter(PthreadMutexPrivate* mutex) : mutex(mutex) {}

    void unlock() {
        savedCount = mutex->_count;
        mutex->_count = 0;
        mutex->_owner.store(std::thread::id{}, std::memory_order_release);
        mutex->_mtx.unlock();
    }

    void lock() {
        mutex->_mtx.lock();
        mutex->_owner.store(std::this_thread::get_id(), std::memory_order_release);
        mutex->_count = savedCount;
    }

private:
    PthreadMutexPrivate* mutex;
    int savedCount = 0;
};

}

PthreadCondPrivate* CondResolve_nid_no_patch(PthreadCond* cond) {
    if (!cond) return nullptr;
    auto& slot = *reinterpret_cast<std::atomic<PthreadCondPrivate*>*>(cond);
    auto* current = slot.load(std::memory_order_acquire);
    if (current) return current;
    auto* created = new PthreadCondPrivate();
    if (slot.compare_exchange_strong(current, created, std::memory_order_acq_rel)) return created;
    delete created;
    return current;
}

int CondInit_nid_no_patch(PthreadCond* cond, const PthreadCondattr* attr) {
    if (!cond) return kEINVAL;
    auto* created = new (std::nothrow) PthreadCondPrivate();
    if (!created) return kENOMEM;
    if (attr && *attr) created->_clockid = (*attr)->_clockid;
    *cond = created;
    return 0;
}

int CondDestroy_nid_no_patch(PthreadCond* cond) {
    if (!cond) return kEINVAL;
    delete *cond;
    *cond = nullptr;
    return 0;
}

int CondWait_nid_no_patch(PthreadCond* cond, PthreadMutex* mutex, MutexType defaultType, const Deadline* deadline) {
    auto* c = CondResolve_nid_no_patch(cond);
    auto* m = MutexResolve_nid_no_patch(mutex, defaultType);
    if (!c || !m) return kEINVAL;
    if (m->_owner.load(std::memory_order_acquire) != std::this_thread::get_id()) return kEPERM;
    MutexWaitAdapter adapter(m);
    if (!deadline) {
        c->_cv.wait(adapter);
        return 0;
    }
    return c->_cv.wait_until(adapter, *deadline) == std::cv_status::timeout ? kETIMEDOUT : 0;
}

int CondSignal_nid_no_patch(PthreadCond* cond, bool all) {
    auto* c = CondResolve_nid_no_patch(cond);
    if (!c) return kEINVAL;
    if (all) c->_cv.notify_all();
    else c->_cv.notify_one();
    return 0;
}

}

using namespace PthreadSync;

extern "C" {

int APS5_VABI scePthreadCondattrInit(PthreadCondattr* attr) {
    if (!attr) return ToSce(kEINVAL);
    auto* p = new (std::nothrow) PthreadCondattrPrivate{};
    if (!p) return ToSce(kENOMEM);
    *attr = p;
    return 0;
}

int APS5_VABI scePthreadCondattrDestroy(PthreadCondattr* attr) {
    if (!attr || !*attr) return ToSce(kEINVAL);
    delete *attr;
    *attr = nullptr;
    return 0;
}

int APS5_VABI scePthreadCondInit(PthreadCond* cond, const PthreadCondattr* attr, const char*) {
    return ToSce(CondInit_nid_no_patch(cond, attr));
}

int APS5_VABI scePthreadCondDestroy(PthreadCond* cond) {
    return ToSce(CondDestroy_nid_no_patch(cond));
}

int APS5_VABI scePthreadCondSignal(PthreadCond* cond) {
    return ToSce(CondSignal_nid_no_patch(cond, false));
}

int APS5_VABI scePthreadCondBroadcast(PthreadCond* cond) {
    return ToSce(CondSignal_nid_no_patch(cond, true));
}

int APS5_VABI scePthreadCondSignalto(PthreadCond* cond, Pthread thread) {
    (void)thread;
    return ToSce(CondSignal_nid_no_patch(cond, true));
}

int APS5_VABI scePthreadCondWait(PthreadCond* cond, PthreadMutex* mutex) {
    return ToSce(CondWait_nid_no_patch(cond, mutex, MutexType::ErrorCheck, nullptr));
}

int APS5_VABI scePthreadCondTimedwait(PthreadCond* cond, PthreadMutex* mutex, unsigned int usec) {
    const auto deadline = DeadlineFromRelative_nid_no_patch(usec);
    return ToSce(CondWait_nid_no_patch(cond, mutex, MutexType::ErrorCheck, &deadline));
}


}
