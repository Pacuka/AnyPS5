#ifndef CORE_LIBS_PRX_LIBKERNEL_APR_APRCOMMANDS_HPP
#define CORE_LIBS_PRX_LIBKERNEL_APR_APRCOMMANDS_HPP

#include <cstddef>
#include <cstdint>

#include "SceTypes.hpp"

// Encoding of AMPR/APR command buffers. libSceAmpr records commands into guest memory in this
// format and the libkernel APR submission executes them, so both sides must agree on it.

namespace Apr {

// Guest-allocated sce::Ampr::CommandBuffer object; the APR variant adds two 8-byte fields at
// 0x18 and 0x20, which the SDK passes to the APR functions by address.
struct CommandBufferState {
    std::uint8_t* buffer;
    std::uint32_t size;
    std::uint32_t offset;
    std::uint64_t reserved;
};
static_assert(sizeof(CommandBufferState) == 0x18);

enum class CommandType : std::uint32_t {
    ReadFile = 1,
    WriteKernelEventQueue = 2,
};

struct CommandHeader {
    CommandType type;
    std::uint32_t size;
};

struct ReadFileCommand {
    CommandHeader header;
    std::uint32_t fileId;
    std::uint32_t reserved;
    void* destination;
    std::uint64_t size;
    std::uint64_t offset;
};

struct WriteKernelEventQueueCommand {
    CommandHeader header;
    std::int32_t id;
    std::uint32_t flags;
    KernelEqueue queue;
    std::uint64_t data;
};

// Completion record the guest passes to sceKernelAprSubmitCommandBufferAndGetResult.
struct SubmissionResult {
    std::int32_t error;
    std::uint32_t commandBufferOffset;
};

}

#endif
