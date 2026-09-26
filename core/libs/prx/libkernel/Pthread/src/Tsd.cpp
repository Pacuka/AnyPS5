#include "../include/Pthread.hpp"
#include "prx/libc/include/General.hpp"
#include <array>

namespace PthreadSync {

namespace {

constexpr int kKeyCount = 256;
constexpr int kDestructorIterations = 4;
constexpr int kOnceNeverDone = 0;
constexpr int kOnceDone = 1;
constexpr int kOnceInProgress = 2;

struct KeySlot {
    bool used = false;
    pthread_key_destructor_func_t destructor = nullptr;
};

std::mutex& KeyMutex() {
    static std::mutex value;
    return value;
}

std::array<KeySlot, kKeyCount>& Keys() {
    static std::array<KeySlot, kKeyCount> value{};
    return value;
}

thread_local std::array<void*, kKeyCount> values{};

bool IsValidKey(PthreadKey key) {
    return key > 0 && key < kKeyCount;
}

}

int KeyCreate_nid_no_patch(PthreadKey* key, pthread_key_destructor_func_t destructor) {
    if (!key) return kEINVAL;
    std::lock_guard lock(KeyMutex());
    auto& keys = Keys();
    for (int index = 1; index < kKeyCount; ++index) {
        if (keys[index].used) continue;
        keys[index] = {true, destructor};
        *key = index;
        return 0;
    }
    return kEAGAIN;
}

int KeyDelete_nid_no_patch(PthreadKey key) {
    if (!IsValidKey(key)) return kEINVAL;
    std::lock_guard lock(KeyMutex());
    auto& slot = Keys()[key];
    if (!slot.used) return kEINVAL;
    slot = {};
    return 0;
}

void* GetSpecific_nid_no_patch(PthreadKey key) {
    if (!IsValidKey(key)) return nullptr;
    return values[key];
}

int SetSpecific_nid_no_patch(PthreadKey key, const void* value) {
    if (!IsValidKey(key)) return kEINVAL;
    values[key] = const_cast<void*>(value);
    return 0;
}

void RunKeyDestructors_nid_no_patch() {
    for (int iteration = 0; iteration < kDestructorIterations; ++iteration) {
        bool called = false;
        for (int index = 1; index < kKeyCount; ++index) {
            void* value = values[index];
            if (!value) continue;
            pthread_key_destructor_func_t destructor = nullptr;
            {
                std::lock_guard lock(KeyMutex());
                if (Keys()[index].used) destructor = Keys()[index].destructor;
            }
            values[index] = nullptr;
            if (!destructor) continue;
            destructor(value);
            called = true;
        }
        if (!called) return;
    }
}

int Once_nid_no_patch(int* state, void (APS5_VABI *routine)()) {
    if (!state || !routine) return kEINVAL;
    auto& atomicState = *reinterpret_cast<std::atomic<int>*>(state);
    for (;;) {
        int current = atomicState.load(std::memory_order_acquire);
        if (current == kOnceDone) return 0;
        if (current == kOnceNeverDone && atomicState.compare_exchange_strong(current, kOnceInProgress, std::memory_order_acq_rel)) {
            routine();
            atomicState.store(kOnceDone, std::memory_order_release);
            return 0;
        }
        std::this_thread::yield();
    }
}

}

using namespace PthreadSync;

extern "C" {

int APS5_VABI scePthreadKeyCreate(PthreadKey* key, pthread_key_destructor_func_t destructor) {
    return ToSce(KeyCreate_nid_no_patch(key, destructor));
}

int APS5_VABI scePthreadKeyDelete(PthreadKey key) {
    return ToSce(KeyDelete_nid_no_patch(key));
}

void* APS5_VABI scePthreadGetspecific(PthreadKey key) {
    return GetSpecific_nid_no_patch(key);
}

int APS5_VABI scePthreadSetspecific(PthreadKey key, void* value) {
    return ToSce(SetSpecific_nid_no_patch(key, value));
}

int APS5_VABI scePthreadOnce(int* once, void (APS5_VABI *routine)()) {
    return ToSce(Once_nid_no_patch(once, routine));
}

}
