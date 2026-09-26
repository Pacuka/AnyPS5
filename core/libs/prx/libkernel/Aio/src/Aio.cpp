#include <cerrno>
#include <cstdint>
#include <cstddef>
#include <cstring>
#include <mutex>
#include <unordered_map>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/GuestErrno.hpp"
#include "prx/libkernel/File/include/File.hpp"

// Requests are executed synchronously when submitted, so every request is already complete when
// its id is handed back; waits and polls only report that state.

namespace {

constexpr int SCE_KERNEL_AIO_STATE_COMPLETED = 3;

struct AioSchedulingParam {
    std::int32_t schedulingWindowSize;
    std::int32_t delayedCountLimit;
    std::uint32_t enableSplit;
    std::uint32_t splitSize;
    std::uint32_t splitChunkSize;
};

struct AioParam {
    AioSchedulingParam low;
    AioSchedulingParam mid;
    AioSchedulingParam high;
};

std::mutex requestsLock;
std::unordered_map<std::int32_t, int> requests;
std::int32_t nextRequestId = 1;

extern "C" std::int64_t APS5_VABI sceKernelPread(int d, void* buf, size_t nbytes, int64_t offset);
extern "C" std::int64_t APS5_VABI sceKernelPwrite(int d, const void* buf, size_t nbytes, int64_t offset);

template<typename TTransfer>
int submit(KernelAioRwRequest* req, int32_t size, int32_t* id, TTransfer transfer) {
    if (!req || !id || size <= 0) return SceKernelError(EINVAL);
    for (int32_t index = 0; index < size; ++index) {
        if (!req[index].result) return SceKernelError(EINVAL);
    }
    for (int32_t index = 0; index < size; ++index) {
        auto& request = req[index];
        request.result->return_value = transfer(request);
        request.result->state = SCE_KERNEL_AIO_STATE_COMPLETED;
    }
    std::lock_guard lock(requestsLock);
    *id = nextRequestId++;
    requests.emplace(*id, SCE_KERNEL_AIO_STATE_COMPLETED);
    return 0;
}

int stateOf(int32_t id, int* state) {
    std::lock_guard lock(requestsLock);
    const auto found = requests.find(id);
    if (found == requests.end()) return SceKernelError(ESRCH);
    *state = found->second;
    return 0;
}

}

extern "C" {

int APS5_VABI sceKernelAioDeleteRequest(int32_t id, int32_t* ret) {
    std::lock_guard lock(requestsLock);
    if (requests.erase(id) == 0) return SceKernelError(ESRCH);
    if (ret) *ret = 0;
    return 0;
}

int APS5_VABI sceKernelAioInitializeImpl(void* param, int32_t size) {
    (void)param;
    (void)size;
    return 0;
}

void APS5_VABI sceKernelAioInitializeParam(void* param) {
    if (!param) return;
    const AioSchedulingParam scheduling{0x20, 0x20, 1, 0x100000, 0x100000};
    const AioParam defaults{scheduling, scheduling, scheduling};
    std::memcpy(param, &defaults, sizeof(defaults));
}

int APS5_VABI sceKernelAioSubmitReadCommands(KernelAioRwRequest* req, int32_t size, int32_t prio, int32_t* id) {
    (void)prio;
    return submit(req, size, id, [](KernelAioRwRequest& request) { return sceKernelPread(request.fd, request.buf, request.nbyte, request.offset); });
}

int APS5_VABI sceKernelAioSubmitWriteCommands(KernelAioRwRequest* req, int32_t size, int32_t prio, int32_t* id) {
    (void)prio;
    return submit(req, size, id, [](KernelAioRwRequest& request) { return sceKernelPwrite(request.fd, request.buf, request.nbyte, request.offset); });
}

int APS5_VABI sceKernelAioWaitRequest(int32_t id, int32_t* state, uint32_t* usec) {
    (void)usec;
    int current = 0;
    if (const int result = stateOf(id, &current); result != 0) return result;
    if (state) *state = current;
    return 0;
}

int APS5_VABI sceKernelAioPollRequests(const int32_t* ids, int32_t count, int32_t* states) {
    if (!ids || !states || count <= 0) return SceKernelError(EINVAL);
    for (int32_t index = 0; index < count; ++index) {
        if (const int result = stateOf(ids[index], &states[index]); result != 0) return result;
    }
    return 0;
}

}
