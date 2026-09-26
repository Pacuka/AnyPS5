#include <cstdint>
#include <cstddef>
#include <cstring>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

// The whole application is installed on the host, so it is reported as a single chunk (id 0)
// that is already available on fast local storage.

namespace {

constexpr int SCE_PLAYGO_ERROR_INVALID_ARGUMENT = static_cast<int>(0x80B2000B);
constexpr int SCE_PLAYGO_ERROR_NOT_INITIALIZED = static_cast<int>(0x80B2000C);
constexpr int SCE_PLAYGO_ERROR_ALREADY_INITIALIZED = static_cast<int>(0x80B2000D);
constexpr int SCE_PLAYGO_ERROR_BAD_HANDLE = static_cast<int>(0x80B20010);
constexpr int SCE_PLAYGO_ERROR_BAD_POINTER = static_cast<int>(0x80B20011);
constexpr int SCE_PLAYGO_ERROR_BAD_SIZE = static_cast<int>(0x80B20012);
constexpr int SCE_PLAYGO_ERROR_BAD_CHUNK_ID = static_cast<int>(0x80B20013);
constexpr int SCE_PLAYGO_ERROR_BAD_SPEED = static_cast<int>(0x80B20014);
constexpr int SCE_PLAYGO_ERROR_BAD_LOCUS = static_cast<int>(0x80B20017);

constexpr int8_t SCE_PLAYGO_LOCUS_LOCAL_FAST = 3;
constexpr int32_t SCE_PLAYGO_INSTALL_SPEED_SUSPENDED = 0;
constexpr int32_t SCE_PLAYGO_INSTALL_SPEED_FULL = 2;
constexpr uint16_t INSTALLED_CHUNK = 0;
constexpr int PLAYGO_HANDLE = 1;

bool initialized = false;
bool opened = false;
int32_t installSpeed = SCE_PLAYGO_INSTALL_SPEED_FULL;

int checkHandle(int handle) {
    if (!initialized) return SCE_PLAYGO_ERROR_NOT_INITIALIZED;
    if (!opened || handle != PLAYGO_HANDLE) return SCE_PLAYGO_ERROR_BAD_HANDLE;
    return 0;
}

int checkChunks(const uint16_t* chunk_ids, uint32_t number_of_entries) {
    if (!chunk_ids) return SCE_PLAYGO_ERROR_BAD_POINTER;
    if (number_of_entries == 0) return SCE_PLAYGO_ERROR_BAD_SIZE;
    for (uint32_t index = 0; index < number_of_entries; ++index) {
        if (chunk_ids[index] != INSTALLED_CHUNK) return SCE_PLAYGO_ERROR_BAD_CHUNK_ID;
    }
    return 0;
}

int listInstalledChunk(int handle, uint16_t* out_chunk_id_list, uint32_t number_of_entries, uint32_t* out_entries) {
    if (const int result = checkHandle(handle); result != 0) return result;
    if (!out_entries) return SCE_PLAYGO_ERROR_BAD_POINTER;
    if (!out_chunk_id_list) {
        *out_entries = 1;
        return 0;
    }
    if (number_of_entries == 0) return SCE_PLAYGO_ERROR_BAD_SIZE;
    out_chunk_id_list[0] = INSTALLED_CHUNK;
    *out_entries = 1;
    return 0;
}

int optionalChunk(int handle, PlayGoOptionalChunk* option) {
    if (const int result = checkHandle(handle); result != 0) return result;
    if (!option) return SCE_PLAYGO_ERROR_BAD_POINTER;
    option->bitmask = 0;
    return 0;
}

}

extern "C" {

int APS5_VABI scePlayGoInitialize(const PlayGoInitParams* init) {
    if (!init) return SCE_PLAYGO_ERROR_BAD_POINTER;
    if (initialized) return SCE_PLAYGO_ERROR_ALREADY_INITIALIZED;
    initialized = true;
    return 0;
}

int APS5_VABI scePlayGoTerminate(void) {
    if (!initialized) return SCE_PLAYGO_ERROR_NOT_INITIALIZED;
    initialized = false;
    opened = false;
    return 0;
}

int APS5_VABI scePlayGoOpen(int* out_handle, const void* param) {
    (void)param;
    if (!initialized) return SCE_PLAYGO_ERROR_NOT_INITIALIZED;
    if (!out_handle) return SCE_PLAYGO_ERROR_BAD_POINTER;
    opened = true;
    *out_handle = PLAYGO_HANDLE;
    return 0;
}

int APS5_VABI scePlayGoClose(int handle) {
    if (const int result = checkHandle(handle); result != 0) return result;
    opened = false;
    return 0;
}

int APS5_VABI scePlayGoGetChunkId(int handle, uint16_t* out_chunk_id_list, uint32_t number_of_entries, uint32_t* out_entries) {
    return listInstalledChunk(handle, out_chunk_id_list, number_of_entries, out_entries);
}

int APS5_VABI scePlayGoGetInstallChunkId(int handle, uint16_t* out_chunk_id_list, uint32_t number_of_entries, uint32_t* out_entries) {
    return listInstalledChunk(handle, out_chunk_id_list, number_of_entries, out_entries);
}

int APS5_VABI scePlayGoGetEta(int handle, const uint16_t* chunk_ids, uint32_t number_of_entries, int64_t* out_eta) {
    if (const int result = checkHandle(handle); result != 0) return result;
    if (!out_eta) return SCE_PLAYGO_ERROR_BAD_POINTER;
    if (const int result = checkChunks(chunk_ids, number_of_entries); result != 0) return result;
    *out_eta = 0;
    return 0;
}

int APS5_VABI scePlayGoGetInstallSpeed(int handle, int32_t* out_speed) {
    if (const int result = checkHandle(handle); result != 0) return result;
    if (!out_speed) return SCE_PLAYGO_ERROR_BAD_POINTER;
    *out_speed = installSpeed;
    return 0;
}

int APS5_VABI scePlayGoSetInstallSpeed(int handle, int32_t speed) {
    if (const int result = checkHandle(handle); result != 0) return result;
    if (speed < SCE_PLAYGO_INSTALL_SPEED_SUSPENDED || speed > SCE_PLAYGO_INSTALL_SPEED_FULL) return SCE_PLAYGO_ERROR_BAD_SPEED;
    installSpeed = speed;
    return 0;
}

int APS5_VABI scePlayGoGetLanguageMask(int handle, uint64_t* out_language_mask) {
    if (const int result = checkHandle(handle); result != 0) return result;
    if (!out_language_mask) return SCE_PLAYGO_ERROR_BAD_POINTER;
    *out_language_mask = ~uint64_t{0};
    return 0;
}

int APS5_VABI scePlayGoGetLocus(int handle, const uint16_t* chunk_ids, uint32_t number_of_entries, int8_t* out_loci) {
    if (const int result = checkHandle(handle); result != 0) return result;
    if (!out_loci) return SCE_PLAYGO_ERROR_BAD_POINTER;
    if (const int result = checkChunks(chunk_ids, number_of_entries); result != 0) return result;
    std::memset(out_loci, SCE_PLAYGO_LOCUS_LOCAL_FAST, number_of_entries);
    return 0;
}

int APS5_VABI scePlayGoGetProgress(int handle, const uint16_t* chunk_ids, uint32_t number_of_entries, PlayGoProgress* out_progress) {
    if (const int result = checkHandle(handle); result != 0) return result;
    if (!out_progress) return SCE_PLAYGO_ERROR_BAD_POINTER;
    if (const int result = checkChunks(chunk_ids, number_of_entries); result != 0) return result;
    out_progress->progress_size = number_of_entries;
    out_progress->total_size = number_of_entries;
    return 0;
}

int APS5_VABI scePlayGoGetOptionalChunk(int handle, int32_t type, PlayGoOptionalChunk* option) {
    (void)type;
    return optionalChunk(handle, option);
}

int APS5_VABI scePlayGoGetSupportedOptionalChunk(int handle, int32_t type, PlayGoOptionalChunk* option) {
    (void)type;
    return optionalChunk(handle, option);
}

int APS5_VABI scePlayGoGetToDoList(int handle, PlayGoToDo* out_todo_list, uint32_t number_of_entries, uint32_t* out_entries) {
    (void)number_of_entries;
    if (const int result = checkHandle(handle); result != 0) return result;
    if (!out_todo_list || !out_entries) return SCE_PLAYGO_ERROR_BAD_POINTER;
    *out_entries = 0;
    return 0;
}

int APS5_VABI scePlayGoSetToDoList(int handle, const PlayGoToDo* todo_list, uint32_t number_of_entries) {
    if (const int result = checkHandle(handle); result != 0) return result;
    if (!todo_list) return SCE_PLAYGO_ERROR_BAD_POINTER;
    for (uint32_t index = 0; index < number_of_entries; ++index) {
        if (todo_list[index].chunk_id != INSTALLED_CHUNK) return SCE_PLAYGO_ERROR_BAD_CHUNK_ID;
    }
    return 0;
}

int APS5_VABI scePlayGoPrefetch(int handle, const uint16_t* chunk_ids, uint32_t number_of_entries, int8_t minimum_locus) {
    if (const int result = checkHandle(handle); result != 0) return result;
    if (minimum_locus < 0 || minimum_locus > SCE_PLAYGO_LOCUS_LOCAL_FAST) return SCE_PLAYGO_ERROR_BAD_LOCUS;
    return checkChunks(chunk_ids, number_of_entries);
}

int APS5_VABI scePlayGoPrefetchOptionalChunk(int handle, int32_t type, const PlayGoOptionalChunk* option) {
    (void)type;
    if (const int result = checkHandle(handle); result != 0) return result;
    return option ? 0 : SCE_PLAYGO_ERROR_INVALID_ARGUMENT;
}


// The package's chunk table is not part of the dump, so the chunk set comes from the title's
// playgo-chunkdefs.xml: every listed chunk plus chunks 0 through the default chunk. Games probe
// chunk IDs and size arrays from the result, so unknown IDs must be rejected.
}
