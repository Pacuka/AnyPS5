#include <cstdint>
#include <cstddef>
#include <cstring>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

// Broadcasting is unavailable, so the game is never on air.

namespace {

constexpr int SCE_GAME_LIVE_STREAMING_ERROR_NOT_INITIALIZED = static_cast<int>(0x80A40001);
constexpr int SCE_GAME_LIVE_STREAMING_ERROR_INVALID_ARGUMENT = static_cast<int>(0x80A40002);
constexpr int SCE_GAME_LIVE_STREAMING_ERROR_NOT_ON_AIR = static_cast<int>(0x80A40007);
// Leading part of SceGameLiveStreamingStatus2 up to and including the on-air flag and viewers.
constexpr std::size_t STATUS_REPORTED_BYTES = 0x20;

bool initialized = false;

}

extern "C" {

int APS5_VABI sceGameLiveStreamingInitialize(size_t heap_size) {
 (void)heap_size;
 initialized = true;
 return 0;
}

int APS5_VABI sceGameLiveStreamingTerminate(void) {
 if (!initialized) return SCE_GAME_LIVE_STREAMING_ERROR_NOT_INITIALIZED;
 initialized = false;
 return 0;
}

}

extern "C" {

int APS5_VABI sceGameLiveStreamingGetCurrentStatus2_nid_postfix(void* status) {
 if (!status) return SCE_GAME_LIVE_STREAMING_ERROR_INVALID_ARGUMENT;
 if (!initialized) return SCE_GAME_LIVE_STREAMING_ERROR_NOT_INITIALIZED;
 std::memset(status, 0, STATUS_REPORTED_BYTES);
 return 0;
}

int APS5_VABI sceGameLiveStreamingGetProgramInfo_nid_postfix(void* program_info) {
 if (!program_info) return SCE_GAME_LIVE_STREAMING_ERROR_INVALID_ARGUMENT;
 return initialized ? SCE_GAME_LIVE_STREAMING_ERROR_NOT_ON_AIR : SCE_GAME_LIVE_STREAMING_ERROR_NOT_INITIALIZED;
}

}
