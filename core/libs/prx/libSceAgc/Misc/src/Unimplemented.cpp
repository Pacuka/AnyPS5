#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

extern "C" {

int APS5_VABI sceAgcDebugRaiseException_nid_postfix() {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceAgcUnknownKRzWekV120_nid_no_patch() {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

// Reports whether the GPU offers the PS5 Pro ("Trinity") resource features; the game rejects
// resources that request them when the flag is clear. The base PS5 GPU is emulated.
int APS5_VABI sceAgcUnknownBfBDZGbti7A_nid_no_patch(bool* supported) {
    if (supported) *supported = false;
    return 0;
}

int APS5_VABI sceAgcUnknownFPSCdQxgpSw_nid_no_patch() {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

}

APS5_EXPORT("-KRzWekV120", sceAgcUnknownKRzWekV120_nid_no_patch);
APS5_EXPORT("BfBDZGbti7A", sceAgcUnknownBfBDZGbti7A_nid_no_patch);
APS5_EXPORT("fPSCdQxgpSw", sceAgcUnknownFPSCdQxgpSw_nid_no_patch);
