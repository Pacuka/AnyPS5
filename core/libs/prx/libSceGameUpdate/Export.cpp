#include <cstdint>
#include <cstddef>
#include <cstring>
#include <mutex>
#include <set>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

// No update server is reachable, so checks complete without finding a newer version.

namespace {

constexpr int SCE_GAME_UPDATE_ERROR_INVALID_ARGUMENT = static_cast<int>(0x80A70002);
constexpr int SCE_GAME_UPDATE_ERROR_NOT_INITIALIZED = static_cast<int>(0x80A70001);

std::mutex lock;
bool initialized = false;
std::set<int> requests;
int nextRequest = 1;

bool isRequest(int request_id) {
    std::lock_guard guard(lock);
    return requests.count(request_id) != 0;
}

}

extern "C" {

int APS5_VABI sceGameUpdateInitialize(void) {
    std::lock_guard guard(lock);
    initialized = true;
    return 0;
}

int APS5_VABI sceGameUpdateTerminate(void) {
    std::lock_guard guard(lock);
    if (!initialized) return SCE_GAME_UPDATE_ERROR_NOT_INITIALIZED;
    initialized = false;
    requests.clear();
    return 0;
}

int APS5_VABI sceGameUpdateCreateRequest(void) {
    std::lock_guard guard(lock);
    if (!initialized) return SCE_GAME_UPDATE_ERROR_NOT_INITIALIZED;
    const int id = nextRequest++;
    requests.insert(id);
    return id;
}

int APS5_VABI sceGameUpdateDeleteRequest(int request_id) {
    std::lock_guard guard(lock);
    return requests.erase(request_id) == 1 ? 0 : SCE_GAME_UPDATE_ERROR_INVALID_ARGUMENT;
}

int APS5_VABI sceGameUpdateAbortRequest(int request_id) {
    return isRequest(request_id) ? 0 : SCE_GAME_UPDATE_ERROR_INVALID_ARGUMENT;
}

int APS5_VABI sceGameUpdateCheck(int request_id, const GameUpdateCheckParam* param, GameUpdateCheckResult* result) {
    if (!param || !result || !isRequest(request_id)) return SCE_GAME_UPDATE_ERROR_INVALID_ARGUMENT;
    const std::size_t size = result->size;
    std::memset(result, 0, sizeof(*result));
    result->size = size;
    result->found = false;
    result->addcont_found = false;
    return 0;
}

int APS5_VABI sceGameUpdateGetAddcontLatestVersion(uint32_t service_label, const void* entitlement_label, GameUpdateAddcontVersionInfo* info) {
    (void)service_label;
    if (!entitlement_label || !info) return SCE_GAME_UPDATE_ERROR_INVALID_ARGUMENT;
    const std::size_t size = info->size;
    std::memset(info, 0, sizeof(*info));
    info->size = size;
    info->found = false;
    return 0;
}

}
