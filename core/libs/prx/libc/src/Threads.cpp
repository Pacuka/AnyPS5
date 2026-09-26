#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <cstddef>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif

#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

// Dinkumware C11 thread support used by the guest std::mutex, std::condition_variable and std::thread.
// Every handle is passed by address; the guest stores one pointer-sized handle per object.

namespace {

constexpr int THRD_SUCCESS = 0;
constexpr int THRD_TIMEDOUT = 2;
constexpr int THRD_BUSY = 3;
constexpr int THRD_ERROR = 4;
constexpr int MTX_RECURSIVE = 0x100;

struct Mutex {
    int type = 0;
    std::string name;
    std::mutex state;
    std::condition_variable released;
    std::thread::id owner;
    unsigned count = 0;
};

struct Condition {
    std::condition_variable_any waiters;
};

struct Xtime {
    std::int64_t sec;
    long nsec;
};

// Guest std::thread handles are libkernel pthreads; libc cannot link against libkernel, which
// depends on it, so the exports are resolved by NID once the process has loaded both.
void* kernelExport(const char* nid, const char* name) {
#ifdef _WIN32
    const HMODULE kernel = GetModuleHandleA("libkernel.prx");
    void* symbol = kernel ? reinterpret_cast<void*>(GetProcAddress(kernel, nid)) : nullptr;
#else
    void* symbol = dlsym(RTLD_DEFAULT, nid);
#endif
    if (!symbol) throw std::runtime_error(std::string("libc threads: libkernel export ") + name + " is not loaded");
    return symbol;
}

using KernelThread = void*;
using PthreadCreate = int (APS5_VABI *)(KernelThread*, const void*, void* (APS5_VABI *)(void*), void*, const char*);
using PthreadJoin = int (APS5_VABI *)(KernelThread, void**);
using PthreadSelf = KernelThread (APS5_VABI *)();

PthreadCreate pthreadCreate() {
    static const auto function = reinterpret_cast<PthreadCreate>(kernelExport("6UgtwV+0zb4", "scePthreadCreate"));
    return function;
}

PthreadJoin pthreadJoin() {
    static const auto function = reinterpret_cast<PthreadJoin>(kernelExport("onNY9Byn-W8", "scePthreadJoin"));
    return function;
}

PthreadSelf pthreadSelf() {
    static const auto function = reinterpret_cast<PthreadSelf>(kernelExport("aI+OeCz8xrQ", "scePthreadSelf"));
    return function;
}

Mutex* resolve(Mutex** handle) {
    if (!handle || !*handle) throw std::invalid_argument("_Mtx: uninitialized mutex");
    return *handle;
}

Condition* resolve(Condition** handle) {
    if (!handle || !*handle) throw std::invalid_argument("_Cnd: uninitialized condition variable");
    return *handle;
}

int unlock(Mutex* mutex, std::unique_lock<std::mutex>&) {
    if (mutex->owner != std::this_thread::get_id()) return THRD_ERROR;
    if (--mutex->count == 0) {
        mutex->owner = {};
        mutex->released.notify_one();
    }
    return THRD_SUCCESS;
}

void acquire(Mutex* mutex, std::unique_lock<std::mutex>& lock, unsigned count) {
    mutex->released.wait(lock, [mutex] { return mutex->owner == std::thread::id{}; });
    mutex->owner = std::this_thread::get_id();
    mutex->count = count;
}

// Releases the guest mutex completely while waiting and restores its recursion depth afterwards.
template<typename TWait>
int waitCondition(Condition* condition, Mutex* mutex, TWait wait) {
    std::unique_lock lock(mutex->state);
    if (mutex->owner != std::this_thread::get_id()) return THRD_ERROR;
    const unsigned count = std::exchange(mutex->count, 0);
    mutex->owner = {};
    mutex->released.notify_one();
    const bool signalled = wait(condition->waiters, lock);
    acquire(mutex, lock, count);
    return signalled ? THRD_SUCCESS : THRD_TIMEDOUT;
}

std::chrono::system_clock::time_point deadline(const Xtime* time) {
    if (!time) throw std::invalid_argument("_Cnd_timedwait: null deadline");
    return std::chrono::system_clock::time_point(std::chrono::duration_cast<std::chrono::system_clock::duration>(
        std::chrono::seconds(time->sec) + std::chrono::nanoseconds(time->nsec)));
}

// Layout of the guest std::_Pad: the derived launch pad's virtual _Go() is the first vtable slot.
struct Pad {
    unsigned (APS5_VABI **vtable)(Pad*);
    Condition* condition;
    Mutex* mutex;
    bool started;
};
static_assert(offsetof(Pad, condition) == 8 && offsetof(Pad, mutex) == 0x10 && offsetof(Pad, started) == 0x18);

void* APS5_VABI runPad(void* argument) {
    auto* pad = static_cast<Pad*>(argument);
    return reinterpret_cast<void*>(static_cast<std::uintptr_t>(pad->vtable[0](pad)));
}

struct ThreadExitDestructors {
    std::vector<std::pair<void (APS5_VABI *)(void*), void*>> entries;

    ~ThreadExitDestructors() {
        while (!entries.empty()) {
            const auto [destructor, object] = entries.back();
            entries.pop_back();
            destructor(object);
        }
    }
};

thread_local ThreadExitDestructors threadExitDestructors;

}

extern "C" {

int APS5_VABI _Mtx_init_with_name_nid_postfix(Mutex** handle, int type, const char* name) {
    if (!handle) throw std::invalid_argument("_Mtx_init: null mutex");
    auto* mutex = new Mutex();
    mutex->type = type;
    if (name) mutex->name = name;
    *handle = mutex;
    return THRD_SUCCESS;
}

int APS5_VABI _Mtx_init_nid_postfix(Mutex** handle, int type) {
    return _Mtx_init_with_name_nid_postfix(handle, type, nullptr);
}

void APS5_VABI _Mtx_destroy_nid_postfix(Mutex** handle) {
    auto* mutex = resolve(handle);
    {
        std::lock_guard lock(mutex->state);
        if (mutex->owner != std::thread::id{}) throw std::runtime_error("_Mtx_destroy: mutex is still locked");
    }
    delete mutex;
    *handle = nullptr;
}

int APS5_VABI _Mtx_lock_nid_postfix(Mutex** handle) {
    auto* mutex = resolve(handle);
    std::unique_lock lock(mutex->state);
    if (mutex->owner == std::this_thread::get_id()) {
        if ((mutex->type & MTX_RECURSIVE) == 0) return THRD_BUSY;
        ++mutex->count;
        return THRD_SUCCESS;
    }
    acquire(mutex, lock, 1);
    return THRD_SUCCESS;
}

int APS5_VABI _Mtx_unlock_nid_postfix(Mutex** handle) {
    auto* mutex = resolve(handle);
    std::unique_lock lock(mutex->state);
    return unlock(mutex, lock);
}

int APS5_VABI _Cnd_init_nid_postfix(Condition** handle) {
    if (!handle) throw std::invalid_argument("_Cnd_init: null condition variable");
    *handle = new Condition();
    return THRD_SUCCESS;
}

void APS5_VABI _Cnd_destroy_nid_postfix(Condition** handle) {
    delete resolve(handle);
    *handle = nullptr;
}

int APS5_VABI _Cnd_wait_nid_postfix(Condition** condition, Mutex** mutex) {
    return waitCondition(resolve(condition), resolve(mutex), [](std::condition_variable_any& waiters, std::unique_lock<std::mutex>& lock) {
        waiters.wait(lock);
        return true;
    });
}

int APS5_VABI _Cnd_timedwait_nid_postfix(Condition** condition, Mutex** mutex, const Xtime* time) {
    const auto until = deadline(time);
    return waitCondition(resolve(condition), resolve(mutex), [until](std::condition_variable_any& waiters, std::unique_lock<std::mutex>& lock) {
        return waiters.wait_until(lock, until) == std::cv_status::no_timeout;
    });
}

int APS5_VABI _Cnd_signal_nid_postfix(Condition** condition) {
    resolve(condition)->waiters.notify_one();
    return THRD_SUCCESS;
}

int APS5_VABI _Cnd_broadcast_nid_postfix(Condition** condition) {
    resolve(condition)->waiters.notify_all();
    return THRD_SUCCESS;
}

// Microseconds since the epoch, the unit the guest uses to build _Cnd_timedwait deadlines.
std::int64_t APS5_VABI _Xtime_get_ticks_nid_postfix() {
    return std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
}

KernelThread APS5_VABI _Thrd_id_nid_postfix() {
    return pthreadSelf()();
}

int APS5_VABI _Thrd_join_nid_postfix(KernelThread thread, int* result) {
    void* value = nullptr;
    if (pthreadJoin()(thread, &value) != 0) return THRD_ERROR;
    if (result) *result = static_cast<int>(reinterpret_cast<std::intptr_t>(value));
    return THRD_SUCCESS;
}

void APS5_VABI _Thrd_yield_nid_postfix() {
    std::this_thread::yield();
}

unsigned APS5_VABI _ZNSt6thread20hardware_concurrencyEv_nid_postfix() {
    return std::thread::hardware_concurrency();
}

void APS5_VABI _ZNSt4_PadC2Ev_nid_postfix(Pad* pad) {
    _Cnd_init_nid_postfix(&pad->condition);
    _Mtx_init_nid_postfix(&pad->mutex, 1);
    pad->started = false;
}

void APS5_VABI _ZNSt4_PadD2Ev_nid_postfix(Pad* pad) {
    _Cnd_destroy_nid_postfix(&pad->condition);
    _Mtx_destroy_nid_postfix(&pad->mutex);
}

// Starts the thread and waits until it has copied its callable out of the pad and called _Release.
void APS5_VABI _ZNSt4_Pad7_LaunchEPP7pthread_nid_postfix(Pad* pad, KernelThread* thread) {
    if (!thread) throw std::invalid_argument("std::thread: null thread handle");
    _Mtx_lock_nid_postfix(&pad->mutex);
    if (pthreadCreate()(thread, nullptr, runPad, pad, "std::thread") != 0) throw std::runtime_error("std::thread: thread creation failed");
    while (!pad->started) _Cnd_wait_nid_postfix(&pad->condition, &pad->mutex);
    _Mtx_unlock_nid_postfix(&pad->mutex);
}

void APS5_VABI _ZNSt4_Pad8_ReleaseEv_nid_postfix(Pad* pad) {
    _Mtx_lock_nid_postfix(&pad->mutex);
    pad->started = true;
    _Cnd_signal_nid_postfix(&pad->condition);
    _Mtx_unlock_nid_postfix(&pad->mutex);
}

int APS5_VABI __cxa_thread_atexit_nid_postfix(void (APS5_VABI *destructor)(void*), void* object, void* dsoHandle) {
    (void)dsoHandle;
    if (!destructor) throw std::invalid_argument("__cxa_thread_atexit: null destructor");
    threadExitDestructors.entries.emplace_back(destructor, object);
    return 0;
}

}
