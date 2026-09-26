#include "prx/libSceAgc/Patch/include/Jump.hpp"

#include "prx/libSceAgc/Command/include/Memory.hpp"
#include "prx/libSceAgc/Command/include/Packet.hpp"
#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

extern "C" {

int APS5_VABI sceAgcJumpPatchSetTarget(uint32_t* cmd, const volatile uint32_t* target, uint32_t size_in_dwords) {
    Agc::Command::ValidatePacket(cmd, 0x3fu, 4, __func__);
    const auto guestAddress = reinterpret_cast<std::uintptr_t>(target);
    Agc::Command::CheckAddress(guestAddress, 4, __func__);
    Agc::Command::CheckBits(guestAddress, 0xffffffffffffull, __func__);
    Agc::Command::CheckBits(size_in_dwords, 0xfffffu, __func__);
    cmd[1] = static_cast<std::uint32_t>(guestAddress);
    cmd[2] = static_cast<std::uint32_t>(guestAddress >> 32u);
    cmd[3] = (cmd[3] & ~0xfffffu) | size_in_dwords;
    return 0;
}

}
