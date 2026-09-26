#include <cstdint>
#include <cstddef>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <regex>
#include <sstream>
#include <string>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

namespace {

constexpr int SCE_APP_CONTENT_ERROR_PARAMETER = static_cast<int>(0x80D90002);
constexpr int SCE_APP_CONTENT_ERROR_NOT_MOUNTED = static_cast<int>(0x80D90004);
constexpr int SCE_APP_CONTENT_ERROR_NOT_FOUND = static_cast<int>(0x80D90005);
constexpr int SCE_APP_CONTENT_ERROR_INTERNAL = static_cast<int>(0x80D9000A);

constexpr std::uint32_t APPPARAM_ID_SKU_FLAG = 0;
constexpr std::uint32_t APPPARAM_ID_USER_DEFINED_PARAM_4 = 4;
constexpr std::int32_t SKU_FLAG_FULL = 3;
constexpr std::uint32_t TEMPORARY_DATA_OPTION_FORMAT = 1;

constexpr const char* TEMPORARY_DATA_MOUNT_POINT = "/temp0";
constexpr const char* DOWNLOAD_DATA_MOUNT_POINT = "/download0";

bool initialized = false;
bool temporaryDataMounted = false;

void writeMountPoint(AppContentMountPoint* mountPoint, const char* path) {
    std::memset(mountPoint->data, 0, sizeof(mountPoint->data));
    std::strncpy(mountPoint->data, path, sizeof(mountPoint->data) - 1);
}

bool isMountPoint(const AppContentMountPoint* mountPoint, const char* path) {
    return std::strncmp(mountPoint->data, path, sizeof(mountPoint->data)) == 0;
}

// Reads userDefinedParamN from the application's param.json.
int readUserDefinedParam(std::uint32_t index, std::int32_t* value) {
    std::ifstream file(ResolvePath_nid_no_patch("/app0/sce_sys/param.json"));
    if (!file) return SCE_APP_CONTENT_ERROR_INTERNAL;
    std::stringstream text;
    text << file.rdbuf();
    const std::string content = text.str();
    const std::regex pattern("\"userDefinedParam" + std::to_string(index) + "\"\\s*:\\s*(-?[0-9]+)");
    std::smatch match;
    *value = std::regex_search(content, match, pattern) ? std::stoi(match[1].str()) : 0;
    return 0;
}

int availableSpaceKb(const char* mountPoint, size_t* available) {
    std::error_code error;
    const auto space = std::filesystem::space(ResolvePath_nid_no_patch(mountPoint), error);
    if (error) return SCE_APP_CONTENT_ERROR_INTERNAL;
    *available = static_cast<size_t>(space.available / 1024);
    return 0;
}

int clearDirectory(const std::filesystem::path& directory) {
    std::error_code error;
    for (const auto& entry : std::filesystem::directory_iterator(directory, error)) {
        std::filesystem::remove_all(entry.path(), error);
        if (error) return SCE_APP_CONTENT_ERROR_INTERNAL;
    }
    return error ? SCE_APP_CONTENT_ERROR_INTERNAL : 0;
}

}

extern "C" {

// Additional content is not installed, so there is nothing to mount.
int APS5_VABI sceAppContentAddcontMount(uint32_t service_label, const NpUnifiedEntitlementLabel* entitlement_label, AppContentMountPoint* mount_point) {
    (void)service_label;
    if (!entitlement_label || !mount_point) return SCE_APP_CONTENT_ERROR_PARAMETER;
    return SCE_APP_CONTENT_ERROR_NOT_FOUND;
}

int APS5_VABI sceAppContentAddcontUnmount(const AppContentMountPoint* mount_point) {
    if (!mount_point) return SCE_APP_CONTENT_ERROR_PARAMETER;
    return SCE_APP_CONTENT_ERROR_NOT_MOUNTED;
}

int APS5_VABI sceAppContentAppParamGetInt(uint32_t param_id, int32_t* value) {
    if (!value || param_id > APPPARAM_ID_USER_DEFINED_PARAM_4) return SCE_APP_CONTENT_ERROR_PARAMETER;
    if (param_id == APPPARAM_ID_SKU_FLAG) {
        *value = SKU_FLAG_FULL;
        return 0;
    }
    return readUserDefinedParam(param_id, value);
}

int APS5_VABI sceAppContentDownloadDataGetAvailableSpaceKb(const AppContentMountPoint* mount_point, size_t* available_space_kb) {
    if (!mount_point || !available_space_kb) return SCE_APP_CONTENT_ERROR_PARAMETER;
    if (!isMountPoint(mount_point, DOWNLOAD_DATA_MOUNT_POINT)) return SCE_APP_CONTENT_ERROR_NOT_MOUNTED;
    return availableSpaceKb(DOWNLOAD_DATA_MOUNT_POINT, available_space_kb);
}

// The download data area is always mounted at /download0, backed by a directory next to the game.
int APS5_VABI sceAppContentInitialize(const AppContentInitParam* init_param, AppContentBootParam* boot_param) {
    if (!init_param || !boot_param) return SCE_APP_CONTENT_ERROR_PARAMETER;
    std::error_code error;
    std::filesystem::create_directories(ResolvePath_nid_no_patch(DOWNLOAD_DATA_MOUNT_POINT), error);
    if (error) return SCE_APP_CONTENT_ERROR_INTERNAL;
    std::memset(boot_param, 0, sizeof(*boot_param));
    initialized = true;
    return 0;
}

int APS5_VABI sceAppContentTemporaryDataFormat(const AppContentMountPoint* mount_point) {
    if (!mount_point) return SCE_APP_CONTENT_ERROR_PARAMETER;
    if (!temporaryDataMounted || !isMountPoint(mount_point, TEMPORARY_DATA_MOUNT_POINT)) return SCE_APP_CONTENT_ERROR_NOT_MOUNTED;
    return clearDirectory(ResolvePath_nid_no_patch(TEMPORARY_DATA_MOUNT_POINT));
}

int APS5_VABI sceAppContentTemporaryDataGetAvailableSpaceKb(const AppContentMountPoint* mount_point, size_t* available_space_kb) {
    if (!mount_point || !available_space_kb) return SCE_APP_CONTENT_ERROR_PARAMETER;
    if (!temporaryDataMounted || !isMountPoint(mount_point, TEMPORARY_DATA_MOUNT_POINT)) return SCE_APP_CONTENT_ERROR_NOT_MOUNTED;
    return availableSpaceKb(TEMPORARY_DATA_MOUNT_POINT, available_space_kb);
}

int APS5_VABI sceAppContentTemporaryDataMount2(uint32_t option, AppContentMountPoint* mount_point) {
    if (!mount_point || !initialized) return SCE_APP_CONTENT_ERROR_PARAMETER;
    const auto directory = ResolvePath_nid_no_patch(TEMPORARY_DATA_MOUNT_POINT);
    std::error_code error;
    std::filesystem::create_directories(directory, error);
    if (error) return SCE_APP_CONTENT_ERROR_INTERNAL;
    if ((option & TEMPORARY_DATA_OPTION_FORMAT) != 0) {
        if (const int result = clearDirectory(directory); result != 0) return result;
    }
    writeMountPoint(mount_point, TEMPORARY_DATA_MOUNT_POINT);
    temporaryDataMounted = true;
    return 0;
}

}
