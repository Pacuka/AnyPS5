#include "prx/libSceAgc/Misc/include/ShaderFusion.hpp"

#include <cstdint>
#include <algorithm>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

struct SizeAlign {
 uint64_t m_size;
 size_t m_align;
};

namespace {

constexpr int AGC_ERROR_INVALID_SHADER = static_cast<int>(0x8a6c000au);
constexpr std::uint8_t FrontHalfType = 4;
constexpr std::uint8_t BackHalfType = 6;

// SPI_SHADER_PGM_LO/HI_GS receive the front half, whose program the back half starts from.
constexpr std::uint32_t PgmLoGs = 0xc8;
constexpr std::uint32_t PgmHiGs = 0xc9;
constexpr std::uint32_t PgmRsrc1Gs = 0x8a;
constexpr std::uint32_t PgmRsrc2Gs = 0x8b;
constexpr std::uint32_t PgmChecksumGs = 0x80;

std::uint32_t mergeRsrc1(std::uint32_t front, std::uint32_t back) {
    const auto vgprs = std::max(front & 0x3fu, back & 0x3fu);
    const auto sgprs = std::max((front >> 6u) & 0xfu, (back >> 6u) & 0xfu);
    return ((front | back) & ~0x3ffu) | vgprs | (sgprs << 6u);
}

// User SGPRs and the vertex input VGPR count belong to the front half, LDS and the rest to the back.
std::uint32_t mergeRsrc2(std::uint32_t front, std::uint32_t back) {
    constexpr std::uint32_t userSgprs = 0x3eu | (1u << 27u);
    constexpr std::uint32_t esVgprCount = 3u << 16u;
    const auto vgprCount = std::max(front & esVgprCount, back & esVgprCount);
    return (back & ~(userSgprs | esVgprCount)) | (front & userSgprs) | vgprCount | (front & 1u);
}

std::size_t alignUp(std::size_t value, std::size_t alignment) {
    return (value + alignment - 1u) & ~(alignment - 1u);
}

std::size_t fusedScratchSize(const Shader* front, const Shader* back) {
    const auto registers = static_cast<std::size_t>(front->num_sh_registers + back->num_sh_registers + front->num_cx_registers + back->num_cx_registers);
    return alignUp(registers * sizeof(ShaderRegister), alignof(ShaderSpecialRegs)) + sizeof(ShaderSpecialRegs);
}

bool fusable(const Shader* front, const Shader* back) {
    return front->type == FrontHalfType && back->type == BackHalfType && back->specials != nullptr && front->specials != nullptr;
}

}

extern "C" {

APS5_EXPORT("dolOmWH+huQ", sceAgcUnknownGetFusedShaderSize);
int APS5_VABI sceAgcUnknownGetFusedShaderSize(SizeAlign* dst, const Shader* front, const Shader* back) {
    if (dst == nullptr || front == nullptr || back == nullptr || !fusable(front, back)) return AGC_ERROR_INVALID_SHADER;
    dst->m_size = fusedScratchSize(front, back);
    dst->m_align = 16;
    return 0;
}

// Fuses a front (vertex) half and a back (geometry) half into one GS-stage shader: the back half's
// registers with the front program in SPI_SHADER_PGM_LO/HI_GS and merged resource words. Both
// programs keep their own code; the register lists and specials live in the caller's scratch memory.
APS5_EXPORT("fd5Bp5tGTgo", sceAgcUnknownFuseShaderHalves);
int APS5_VABI sceAgcUnknownFuseShaderHalves(Shader* fused_result, const Shader* front, const Shader* back, void* scratch_mem) {
    if (fused_result == nullptr || front == nullptr || back == nullptr || scratch_mem == nullptr || !fusable(front, back)) return AGC_ERROR_INVALID_SHADER;
    auto* registers = static_cast<ShaderRegister*>(scratch_mem);
    std::size_t count = 0;
    const auto frontCode = reinterpret_cast<std::uintptr_t>(front->code);
    const auto frontValue = [&](std::uint32_t offset) -> const ShaderRegister* {
        for (std::uint32_t i = 0; i < front->num_sh_registers; ++i) if (front->sh_registers[i].offset == offset) return &front->sh_registers[i];
        return nullptr;
    };
    for (std::uint32_t i = 0; i < back->num_sh_registers; ++i) {
        auto reg = back->sh_registers[i];
        if (reg.offset == PgmLoGs) reg.value = static_cast<std::uint32_t>(frontCode >> 8u);
        else if (reg.offset == PgmHiGs) reg.value = static_cast<std::uint32_t>(frontCode >> 40u);
        else if (reg.offset == PgmRsrc1Gs) { if (const auto* f = frontValue(PgmRsrc1Gs)) reg.value = mergeRsrc1(f->value, reg.value); }
        else if (reg.offset == PgmRsrc2Gs) { if (const auto* f = frontValue(PgmRsrc2Gs)) reg.value = mergeRsrc2(f->value, reg.value); }
        registers[count++] = reg;
    }
    for (std::uint32_t i = 0; i < front->num_sh_registers; ++i) {
        const auto& reg = front->sh_registers[i];
        if (reg.offset == PgmChecksumGs) continue;
        bool present = false;
        for (std::size_t j = 0; j < count; ++j) present = present || registers[j].offset == reg.offset;
        if (!present) registers[count++] = reg;
    }
    const auto shCount = count;
    for (std::uint32_t i = 0; i < back->num_cx_registers; ++i) registers[count++] = back->cx_registers[i];
    for (std::uint32_t i = 0; i < front->num_cx_registers; ++i) registers[count++] = front->cx_registers[i];
    auto* specials = reinterpret_cast<ShaderSpecialRegs*>(static_cast<std::byte*>(scratch_mem) + alignUp(count * sizeof(ShaderRegister), alignof(ShaderSpecialRegs)));
    *specials = *back->specials;
    specials->user_data_range = front->specials->user_data_range;

    Shader result = *back;
    result.user_data = front->user_data;
    result.input_semantics = front->input_semantics;
    result.num_input_semantics = front->num_input_semantics;
    result.sh_registers = registers;
    result.num_sh_registers = static_cast<std::uint8_t>(shCount);
    result.cx_registers = registers + shCount;
    result.num_cx_registers = static_cast<std::uint8_t>(count - shCount);
    result.specials = specials;
    result.scratch_size_dw_per_thread = std::max(front->scratch_size_dw_per_thread, back->scratch_size_dw_per_thread);
    result.embedded_constant_buffer_size_dqw = std::max(front->embedded_constant_buffer_size_dqw, back->embedded_constant_buffer_size_dqw);
    *fused_result = result;
    return 0;
}

APS5_EXPORT("k0E7vkgqAuE", sceAgcCreateInterpolantMappingVsPs);
int APS5_VABI sceAgcCreateInterpolantMappingVsPs(ShaderRegister* regs, const Shader* vs, const Shader* ps) {
    (void)regs;
    (void)vs;
    (void)ps;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

}
