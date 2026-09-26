#include "prx/libSceAgc/DcbState/include/Predication.hpp"

#include "prx/libSceAgc/Command/include/Packet.hpp"
#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

extern "C" {

// SET_PREDICATION: operation 0 clears predication, 3 and 4 test a 64- or 32-bit value at the
// address; condition selects whether predicated packets run when it is nonzero or zero.
std::uint32_t* APS5_VABI sceAgcDcbSetPredication(CommandBuffer* buf, std::uint8_t condition, std::uint8_t op, std::uint8_t wait_op, const volatile void* address, std::uint32_t count_in_dwords) {
    (void)count_in_dwords;
    Agc::Command::CheckBits(condition, 1, __func__);
    Agc::Command::CheckBits(wait_op, 1, __func__);
    Agc::Command::Require(op == 0 || op == 3 || op == 4, __func__, "occlusion and primitive-count predication are not implemented");
    const auto guestAddress = op == 0 ? 0 : reinterpret_cast<std::uintptr_t>(address);
    if (op != 0) {
        Agc::Command::CheckAddress(guestAddress, op == 3 ? 8 : 4, __func__);
        Agc::Command::CheckBits(guestAddress, 0xffffffffffffull, __func__);
    }
    const auto control = (static_cast<std::uint32_t>(condition) << 8u) | (static_cast<std::uint32_t>(wait_op) << 12u) | (static_cast<std::uint32_t>(op) << 16u);
    return Agc::Command::Emit(buf, 0x20u, {control, static_cast<std::uint32_t>(guestAddress), static_cast<std::uint32_t>(guestAddress >> 32u)}, __func__);
}

std::uint32_t APS5_VABI sceAgcDcbSetZPassPredicationEnableGetSize() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

std::uint32_t APS5_VABI sceAgcDcbSetPredicationDisableGetSize() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

std::uint32_t APS5_VABI sceAgcDcbSetBoolPredicationEnableGetSize() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

}
