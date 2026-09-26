#include "prx/libSceAgc/DcbFlow/include/Control.hpp"

#include "prx/libSceAgc/Command/include/Memory.hpp"
#include "prx/libSceAgc/Command/include/Packet.hpp"
#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"


extern "C" {

// unknown signature
APS5_EXPORT("zARR5aCmkoY", sceAgcDcbA_zARR5aCmkoY);
void* APS5_VABI sceAgcDcbA_zARR5aCmkoY(void) {
 NotImplemented_nid_no_patch(__func__);
 return nullptr;
}


// A chained INDIRECT_BUFFER: execution continues in the target and does not return.
std::uint32_t* APS5_VABI sceAgcDcbJump(CommandBuffer* buf, std::uint8_t mode, std::uint8_t cache_policy, const std::uint32_t* target, std::uint32_t size_in_dwords) {
    Agc::Command::Require(mode == 0, __func__, "jump modes other than 0 are not implemented");
    Agc::Command::CheckBits(cache_policy, 3, __func__);
    const auto guestAddress = reinterpret_cast<std::uintptr_t>(target);
    if (guestAddress != 0) Agc::Command::CheckAddress(guestAddress, 4, __func__);
    Agc::Command::CheckBits(guestAddress, 0xffffffffffffull, __func__);
    Agc::Command::CheckBits(size_in_dwords, 0xfffffu, __func__);
    return Agc::Command::Emit(buf, 0x3fu, {static_cast<std::uint32_t>(guestAddress), static_cast<std::uint32_t>(guestAddress >> 32u), size_in_dwords | 0x00900000u | (static_cast<std::uint32_t>(cache_policy) << 28u)}, __func__);
}

std::uint32_t APS5_VABI sceAgcDcbJumpGetSize() {
    return 16;
}

std::uint32_t* APS5_VABI sceAgcDcbResetQueue(CommandBuffer* buf, std::uint32_t op, std::uint32_t state) {
    Agc::Command::CheckBits(op, 0xfffu, __func__);
    Agc::Command::CheckBits(state, 0xfu, __func__);
    return Agc::Command::Emit(buf, 0x12u, {state}, __func__);
}

uint32_t* APS5_VABI sceAgcDcbRewind(CommandBuffer* buf, uint32_t initial_state) {
 (void)buf;
 (void)initial_state;
 NotImplemented_nid_no_patch(__func__);
 return nullptr;
}

uint32_t APS5_VABI sceAgcDcbRewindGetSize(void) {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

uint32_t* APS5_VABI sceAgcDcbWaitUntilSafeForRendering(CommandBuffer* buf, uint32_t video_out_handle, uint32_t display_buffer_index) {
    // Presentation copies the display buffer when the flip executes, so the buffer is always safe to
    // render into; keep the arguments in a NOP for command-buffer dumps.
    return Agc::Command::Emit(buf, 0x10u, {video_out_handle, display_buffer_index}, __func__);
}

}
