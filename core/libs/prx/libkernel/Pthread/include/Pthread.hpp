#ifndef CORE_LIBS_PRX_LIBKERNEL_PTHREAD_PTHREAD_HPP
#define CORE_LIBS_PRX_LIBKERNEL_PTHREAD_PTHREAD_HPP

#include <sched.h>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <shared_mutex>
#include <string>
#include <thread>

enum class MutexType : std::uint32_t {
    ErrorCheck = 1,
    Recursive = 2,
    Normal = 3,
    Adaptive = 4,
};

struct PthreadMutexattrPrivate {
    MutexType type = MutexType::ErrorCheck;
    int protocol = 0;
};

struct PthreadMutexPrivate {
    std::timed_mutex _mtx;
    MutexType _type = MutexType::Normal;
    std::atomic<std::thread::id> _owner{};
    int _count = 0;
};

struct PthreadCondattrPrivate {
    int _clockid = 0;
};

struct PthreadCondPrivate {
    std::condition_variable_any _cv;
    int _clockid = 0;
};

struct PthreadRwlockattrPrivate {
    int type = 0;
};

struct PthreadRwlockPrivate {
    std::shared_timed_mutex _lock;
    std::atomic<std::thread::id> _writer{};
};

struct PthreadAttrPrivate {
    void* stackAddress = nullptr;
    std::size_t _stacksize = 0;
    std::size_t guardSize = 0x1000;
    int _detachstate = 0;
    int _schedpriority = 700;
    int _schedpolicy = 1;
    int _inheritsched = 4;
    int solosched = 0;
    KernelCpumask affinity = 0;
};

struct PthreadPrivate {
#ifdef _WIN32
    void* nativeHandle = nullptr;
    std::thread::id threadId;
    std::atomic<unsigned> references{2};
#else
    std::thread _thr;
#endif
    void* stackAddress = nullptr;
    std::size_t stackSize = 0;
    std::string name;
    int priority = 700;
    KernelCpumask affinity = 0;
    std::atomic<bool> _finished;
    void* _retval;
    bool _detached;
    std::mutex _join_mtx;
    std::condition_variable _join_cv;

    PthreadPrivate() : _finished(false), _retval(nullptr), _detached(false) {}
};

namespace PthreadSync {

inline constexpr int kEPERM = 1;
inline constexpr int kESRCH = 3;
inline constexpr int kEDEADLK = 11;
inline constexpr int kENOMEM = 12;
inline constexpr int kEBUSY = 16;
inline constexpr int kEINVAL = 22;
inline constexpr int kEAGAIN = 35;
inline constexpr int kETIMEDOUT = 60;
inline constexpr int kClockRealtime = 0;
inline constexpr int kClockMonotonic = 4;

using Deadline = std::chrono::steady_clock::time_point;

inline int FromSce_nid_no_patch(int result) {
    return result == 0 ? 0 : static_cast<int>(static_cast<unsigned>(result) & 0xFFFFu);
}

inline int ToSce(int error) {
    return error == 0 ? 0 : static_cast<int>(0x80020000u | static_cast<unsigned>(error));
}

Deadline DeadlineFromAbsolute_nid_no_patch(const KernelTimespec* abstime, int clockId);
Deadline DeadlineFromRelative_nid_no_patch(std::uint64_t microseconds);

bool IsValidMutexType_nid_no_patch(int type);
int MutexInit_nid_no_patch(PthreadMutex* mutex, const PthreadMutexattr* attr, MutexType defaultType);
int MutexDestroy_nid_no_patch(PthreadMutex* mutex);
int MutexLock_nid_no_patch(PthreadMutex* mutex, MutexType defaultType, const Deadline* deadline);
int MutexTrylock_nid_no_patch(PthreadMutex* mutex, MutexType defaultType);
int MutexUnlock_nid_no_patch(PthreadMutex* mutex);
PthreadMutexPrivate* MutexResolve_nid_no_patch(PthreadMutex* mutex, MutexType defaultType);

int CondInit_nid_no_patch(PthreadCond* cond, const PthreadCondattr* attr);
int CondDestroy_nid_no_patch(PthreadCond* cond);
int CondWait_nid_no_patch(PthreadCond* cond, PthreadMutex* mutex, MutexType defaultType, const Deadline* deadline);
int CondSignal_nid_no_patch(PthreadCond* cond, bool all);
PthreadCondPrivate* CondResolve_nid_no_patch(PthreadCond* cond);

int RwlockInit_nid_no_patch(PthreadRwlock* rwlock);
int RwlockDestroy_nid_no_patch(PthreadRwlock* rwlock);
int RwlockRead_nid_no_patch(PthreadRwlock* rwlock, bool tryOnly, const Deadline* deadline);
int RwlockWrite_nid_no_patch(PthreadRwlock* rwlock, bool tryOnly, const Deadline* deadline);
int RwlockUnlock_nid_no_patch(PthreadRwlock* rwlock);

int KeyCreate_nid_no_patch(PthreadKey* key, pthread_key_destructor_func_t destructor);
int KeyDelete_nid_no_patch(PthreadKey key);
void* GetSpecific_nid_no_patch(PthreadKey key);
int SetSpecific_nid_no_patch(PthreadKey key, const void* value);
void RunKeyDestructors_nid_no_patch();

int Once_nid_no_patch(int* state, void (APS5_VABI *routine)());

PthreadPrivate* CurrentThread_nid_no_patch();
void SetCurrentThread_nid_no_patch(PthreadPrivate* thread);

}

#endif
