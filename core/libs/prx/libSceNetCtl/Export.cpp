#include <array>
#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

// The console reports a network that is disabled, so connection state never changes and no
// callbacks fire.

namespace {

constexpr int SCE_NET_CTL_ERROR_NOT_INITIALIZED = static_cast<int>(0x80412101);
constexpr int SCE_NET_CTL_ERROR_CALLBACK_MAX = static_cast<int>(0x80412103);
constexpr int SCE_NET_CTL_ERROR_ID_NOT_FOUND = static_cast<int>(0x80412104);
constexpr int SCE_NET_CTL_ERROR_INVALID_ADDR = static_cast<int>(0x80412107);
constexpr int SCE_NET_CTL_ERROR_NOT_CONNECTED = static_cast<int>(0x80412108);
constexpr int SCE_NET_CTL_ERROR_NETWORK_DISABLED = static_cast<int>(0x8041210D);
constexpr int SCE_NET_CTL_STATE_DISCONNECTED = 0;

struct Callback {
    NetCtlCallback function = nullptr;
    void* argument = nullptr;
};

bool initialized = false;
std::array<Callback, 8> callbacks{};

}

extern "C" {

int APS5_VABI sceNetCtlInit(void) {
    initialized = true;
    return 0;
}

void APS5_VABI sceNetCtlTerm(void) {
    initialized = false;
    callbacks.fill({});
}

int APS5_VABI sceNetCtlCheckCallback(void) {
    return initialized ? 0 : SCE_NET_CTL_ERROR_NOT_INITIALIZED;
}

int APS5_VABI sceNetCtlGetState(int* state) {
    if (!state) return SCE_NET_CTL_ERROR_INVALID_ADDR;
    *state = SCE_NET_CTL_STATE_DISCONNECTED;
    return 0;
}

int APS5_VABI sceNetCtlGetStateV6(int* state) {
    return sceNetCtlGetState(state);
}

int APS5_VABI sceNetCtlGetResult(int event_type, int* error_code) {
    (void)event_type;
    if (!error_code) return SCE_NET_CTL_ERROR_INVALID_ADDR;
    *error_code = SCE_NET_CTL_ERROR_NETWORK_DISABLED;
    return 0;
}

int APS5_VABI sceNetCtlGetInfo(int code, NetCtlInfo* info) {
    (void)code;
    if (!info) return SCE_NET_CTL_ERROR_INVALID_ADDR;
    return SCE_NET_CTL_ERROR_NOT_CONNECTED;
}

int APS5_VABI sceNetCtlGetNatInfo(NetCtlNatInfo* nat_info) {
    if (!nat_info) return SCE_NET_CTL_ERROR_INVALID_ADDR;
    return SCE_NET_CTL_ERROR_NOT_CONNECTED;
}

int APS5_VABI sceNetCtlRegisterCallback(NetCtlCallback func, void* arg, int* cid) {
    if (!func || !cid) return SCE_NET_CTL_ERROR_INVALID_ADDR;
    for (std::size_t index = 0; index < callbacks.size(); ++index) {
        if (callbacks[index].function) continue;
        callbacks[index] = {func, arg};
        *cid = static_cast<int>(index);
        return 0;
    }
    return SCE_NET_CTL_ERROR_CALLBACK_MAX;
}

int APS5_VABI sceNetCtlUnregisterCallback(int cid) {
    if (cid < 0 || cid >= static_cast<int>(callbacks.size()) || !callbacks[cid].function) return SCE_NET_CTL_ERROR_ID_NOT_FOUND;
    callbacks[cid] = {};
    return 0;
}

}
