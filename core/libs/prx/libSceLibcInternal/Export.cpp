#include <cstdint>
#include <cstddef>
#include <algorithm>
#include <cstring>
#include <iterator>
#include <map>
#include <memory>
#include <mutex>
#include <stdexcept>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

extern "C" {


void APS5_VABI sceLibcHeapGetTraceInfo_nid_postfix(Info* info) {
 (void)info;
 NotImplemented_nid_no_patch(__func__);
}

}

namespace {

constexpr std::uintptr_t MSPACE_ALIGNMENT = 16;

struct Mspace {
    std::mutex lock;
    std::size_t capacity = 0;
    std::size_t inUse = 0;
    std::size_t peakInUse = 0;
    std::map<std::uintptr_t, std::size_t> freeBlocks;
    std::map<std::uintptr_t, std::size_t> usedBlocks;
};

struct MallocManagedSize {
    std::uint16_t size;
    std::uint16_t version;
    std::uint32_t reserved;
    std::size_t maxSystemSize;
    std::size_t currentSystemSize;
    std::size_t maxInuseSize;
    std::size_t currentInuseSize;
};

std::mutex mspacesLock;
std::map<Mspace*, std::unique_ptr<Mspace>> mspaces;

Mspace* resolve(void* handle) {
    std::lock_guard guard(mspacesLock);
    const auto found = mspaces.find(static_cast<Mspace*>(handle));
    if (found == mspaces.end()) throw std::invalid_argument("sceLibcMspace: unknown mspace");
    return found->second.get();
}

void* allocate(Mspace& space, std::size_t size) {
    const std::size_t rounded = size == 0 ? MSPACE_ALIGNMENT : (size + MSPACE_ALIGNMENT - 1) & ~(MSPACE_ALIGNMENT - 1);
    for (auto block = space.freeBlocks.begin(); block != space.freeBlocks.end(); ++block) {
        if (block->second < rounded) continue;
        const auto [address, available] = *block;
        space.freeBlocks.erase(block);
        if (available > rounded) space.freeBlocks.emplace(address + rounded, available - rounded);
        space.usedBlocks.emplace(address, rounded);
        space.inUse += rounded;
        space.peakInUse = std::max(space.peakInUse, space.inUse);
        return reinterpret_cast<void*>(address);
    }
    return nullptr;
}

void release(Mspace& space, std::uintptr_t address) {
    const auto used = space.usedBlocks.find(address);
    if (used == space.usedBlocks.end()) throw std::invalid_argument("sceLibcMspaceFree: pointer is not allocated from this mspace");
    std::size_t size = used->second;
    space.usedBlocks.erase(used);
    space.inUse -= size;
    auto next = space.freeBlocks.lower_bound(address);
    if (next != space.freeBlocks.end() && address + size == next->first) {
        size += next->second;
        next = space.freeBlocks.erase(next);
    }
    if (next != space.freeBlocks.begin()) {
        const auto previous = std::prev(next);
        if (previous->first + previous->second == address) {
            previous->second += size;
            return;
        }
    }
    space.freeBlocks.emplace(address, size);
}

}

extern "C" {

using SceLibcMspace = void*;

int Need_sceLibcInternal_nid_postfix = 1;

// A mspace carves allocations out of a caller-provided region. Block bookkeeping lives on the
// host so the region is used for payloads only.
SceLibcMspace APS5_VABI sceLibcMspaceCreate_nid_postfix(const char* name, void* base, size_t capacity, unsigned int flag) {
    (void)name;
    (void)flag;
    const auto begin = (reinterpret_cast<std::uintptr_t>(base) + MSPACE_ALIGNMENT - 1) & ~(MSPACE_ALIGNMENT - 1);
    const auto end = reinterpret_cast<std::uintptr_t>(base) + capacity;
    if (!base || end <= begin) return nullptr;
    auto space = std::make_unique<Mspace>();
    space->capacity = end - begin;
    space->freeBlocks.emplace(begin, end - begin);
    std::lock_guard guard(mspacesLock);
    auto* handle = space.get();
    mspaces.emplace(handle, std::move(space));
    return handle;
}

int APS5_VABI sceLibcMspaceDestroy_nid_postfix(SceLibcMspace msp) {
    std::lock_guard guard(mspacesLock);
    return mspaces.erase(static_cast<Mspace*>(msp)) == 1 ? 0 : -1;
}

void* APS5_VABI sceLibcMspaceMalloc_nid_postfix(SceLibcMspace msp, size_t size) {
    auto* space = resolve(msp);
    std::lock_guard guard(space->lock);
    return allocate(*space, size);
}

int APS5_VABI sceLibcMspaceFree_nid_postfix(SceLibcMspace msp, void* ptr) {
    if (!ptr) return 0;
    auto* space = resolve(msp);
    std::lock_guard guard(space->lock);
    release(*space, reinterpret_cast<std::uintptr_t>(ptr));
    return 0;
}

void* APS5_VABI sceLibcMspaceRealloc_nid_postfix(SceLibcMspace msp, void* ptr, size_t size) {
    auto* space = resolve(msp);
    std::lock_guard guard(space->lock);
    if (!ptr) return allocate(*space, size);
    const auto address = reinterpret_cast<std::uintptr_t>(ptr);
    const auto found = space->usedBlocks.find(address);
    if (found == space->usedBlocks.end()) throw std::invalid_argument("sceLibcMspaceRealloc: pointer is not allocated from this mspace");
    if (size == 0) {
        release(*space, address);
        return nullptr;
    }
    const std::size_t oldSize = found->second;
    void* replacement = allocate(*space, size);
    if (!replacement) return nullptr;
    std::memcpy(replacement, ptr, std::min(oldSize, size));
    release(*space, address);
    return replacement;
}

int APS5_VABI sceLibcMspaceMallocStats_nid_postfix(SceLibcMspace msp, MallocManagedSize* stats) {
    if (!stats) return -1;
    auto* space = resolve(msp);
    std::lock_guard guard(space->lock);
    stats->size = sizeof(MallocManagedSize);
    stats->version = 1;
    stats->reserved = 0;
    stats->maxSystemSize = space->capacity;
    stats->currentSystemSize = space->capacity;
    stats->maxInuseSize = space->peakInUse;
    stats->currentInuseSize = space->inUse;
    return 0;
}

}
