#include "prx/libSceAgc/Predication/include/Predication.hpp"

#include "prx/libSceAgc/Command/include/Packet.hpp"

#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

extern "C" {

int APS5_VABI sceAgcSetPacketPredication(uint32_t* packet, uint32_t predication) {
    Agc::Command::CheckAddress(reinterpret_cast<std::uintptr_t>(packet), 4, __func__);
    Agc::Command::CheckBits(predication, 1, __func__);
    Agc::Command::Require((packet[0] & 0xc0000000u) == 0xc0000000u, __func__, "not a PM4 packet");
    packet[0] = (packet[0] & ~1u) | predication;
    return 0;
}

int APS5_VABI sceAgcSetRangePredication(uint32_t* start, const volatile uint32_t* end, uint32_t predication) {
    Agc::Command::CheckBits(predication, 1, __func__);
    Agc::Command::Require(start != nullptr && start <= end, __func__, "invalid packet range");
    for (auto* packet = start; packet < end; packet += ((packet[0] >> 16u) & 0x3fffu) + 2u) {
        sceAgcSetPacketPredication(packet, predication);
    }
    return 0;
}

}
