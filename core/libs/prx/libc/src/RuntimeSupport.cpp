#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <stdexcept>
#include <functional>
#include <regex>
#include <mutex>
#include <vector>
#include <utility>

#include "prx/libc/include/General.hpp"
#include "prx/libc/include/specifics/gcc/AtomicOps.hpp"
#include "prx/libc/include/FileStream.hpp"

namespace {

std::recursive_mutex g_sysLock;

struct ExitDestructor {
    void (APS5_VABI *function)(void*);
    void* argument;
    void* dsoHandle;
};

std::recursive_mutex g_exitLock;
std::vector<ExitDestructor> g_exitDestructors;

// Runs registered destructors in reverse order; a null handle selects every module.
void runExitDestructors(void* dsoHandle) {
    std::lock_guard lock(g_exitLock);
    for (std::size_t index = g_exitDestructors.size(); index-- > 0;) {
        if (index >= g_exitDestructors.size()) continue;
        const ExitDestructor entry = g_exitDestructors[index];
        if (dsoHandle && entry.dsoHandle != dsoHandle) continue;
        g_exitDestructors.erase(g_exitDestructors.begin() + static_cast<std::ptrdiff_t>(index));
        entry.function(entry.argument);
    }
}

// Guest locale categories follow FreeBSD numbering; only the "C" locale is provided.
constexpr int GUEST_LC_ALL = 0;
constexpr int GUEST_LC_MESSAGES = 6;

}

extern "C" {

FileStream _Stderr_nid_postfix{stderr};
FileStream _Stdout_nid_postfix{stdout};
FileStream _Stdin_nid_postfix{stdin};

int APS5_VABI __cxa_atexit_nid_postfix(void (APS5_VABI *func)(void*), void* arg, void* dsoHandle) {
    if (!func) throw std::invalid_argument("__cxa_atexit: null destructor");
    std::lock_guard lock(g_exitLock);
    static bool runnerRegistered = false;
    g_exitDestructors.push_back({func, arg, dsoHandle});
    if (!runnerRegistered) {
        runnerRegistered = true;
        if (std::atexit([] { runExitDestructors(nullptr); }) != 0) throw std::runtime_error("__cxa_atexit: cannot register exit handler");
    }
    return 0;
}

void APS5_VABI __cxa_finalize_nid_postfix(void* dsoHandle) {
    runExitDestructors(dsoHandle);
}

[[noreturn]] void APS5_VABI quick_exit_nid_postfix(int status) {
    std::fflush(nullptr);
    std::_Exit(status);
}

// The guest exception objects cannot unwind through host frames, so C++ library errors end the
// process with a diagnostic, like the other unrecoverable guest runtime failures.
[[noreturn]] void APS5_VABI _ZSt16_Throw_Cpp_errori_nid_postfix(int code) {
    throw std::runtime_error("guest std::system_error raised by the C++ library, code " + std::to_string(code));
}

[[noreturn]] void APS5_VABI _ZSt14_Throw_C_errori_nid_postfix(int code) {
    throw std::runtime_error("guest C11 thread operation failed with code " + std::to_string(code));
}

unsigned int APS5_VABI _Atomic_fetch_add_4_nid_postfix(volatile unsigned int* target, unsigned int value, int memoryOrder) {
    (void)memoryOrder;
    return GccAtomicFetchAdd(target, value);
}

unsigned int APS5_VABI _Atomic_fetch_sub_4_nid_postfix(volatile unsigned int* target, unsigned int value, int memoryOrder) {
    (void)memoryOrder;
    return GccAtomicFetchSub(target, value);
}

unsigned long APS5_VABI _Stoul_nid_postfix(const char* str, char** endptr, int base) {
    return std::strtoul(str, endptr, base);
}

void APS5_VABI _Locksyslock_nid_postfix() {
    g_sysLock.lock();
}

void APS5_VABI _Unlocksyslock_nid_postfix() {
    g_sysLock.unlock();
}

}
