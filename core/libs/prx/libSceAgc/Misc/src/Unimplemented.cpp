#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libSceAgc/Command/include/Packet.hpp"

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

// Patches the destination address of a WRITE_DATA packet (sceAgcDcbWriteData emitted with a
// null placeholder address), the WriteData counterpart of sceAgcWaitRegMemPatchAddress.
int APS5_VABI sceAgcUnknownFPSCdQxgpSw_nid_no_patch(std::uint32_t* cmd, const volatile void* address) {
    Agc::Command::CheckAddress(reinterpret_cast<std::uintptr_t>(cmd), 4, __func__);
    const auto count = ((cmd[0] >> 16u) & 0x3fffu) + 2u;
    Agc::Command::Require(count > 4u, __func__, "invalid write data packet");
    Agc::Command::ValidatePacket(cmd, 0x37u, count, __func__);
    const auto guestAddress = reinterpret_cast<std::uintptr_t>(address);
    Agc::Command::Require((cmd[1] & 0x40000f00u) == 0 || (guestAddress & 3u) == 0, __func__, "misaligned write address");
    cmd[2] = static_cast<std::uint32_t>(guestAddress);
    cmd[3] = static_cast<std::uint32_t>(guestAddress >> 32u);
    return 0;
}

}

APS5_EXPORT("-KRzWekV120", sceAgcUnknownKRzWekV120_nid_no_patch);
APS5_EXPORT("BfBDZGbti7A", sceAgcUnknownBfBDZGbti7A_nid_no_patch);
APS5_EXPORT("fPSCdQxgpSw", sceAgcUnknownFPSCdQxgpSw_nid_no_patch);
