#include "prx/libSceAgcDriver/Resource/include/Registration.hpp"

#include <cstdint>
#include <cstddef>
#include <map>
#include <mutex>
#include <string>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

// Owner and resource registration feeds GPU debugging tools; the driver keeps the records so
// handles stay valid and unregistration is checked, but nothing else consumes them.

namespace {

constexpr int SCE_AGC_DRIVER_ERROR_INVALID_ARGUMENT = static_cast<int>(0x80D10001);
constexpr int SCE_AGC_DRIVER_ERROR_NOT_FOUND = static_cast<int>(0x80D10002);

struct Resource {
    std::uint32_t owner;
    const void* memory;
    std::size_t size;
    std::string name;
    std::uint32_t type;
};

std::mutex lock;
std::map<std::uint32_t, std::string> owners;
std::map<std::uint32_t, Resource> resources;
std::uint32_t nextHandle = 1;

}

extern "C" {

int APS5_VABI sceAgcDriverRegisterOwner(uint32_t* owner_handle, const char* name) {
    if (!owner_handle) return SCE_AGC_DRIVER_ERROR_INVALID_ARGUMENT;
    std::lock_guard guard(lock);
    *owner_handle = nextHandle++;
    owners.emplace(*owner_handle, name ? name : "");
    return 0;
}

int APS5_VABI sceAgcDriverRegisterResource(uint32_t* resource_handle, uint32_t owner_handle, const void* memory, size_t size, const char* name, uint32_t type, uint64_t user_data) {
    (void)user_data;
    if (!resource_handle) return SCE_AGC_DRIVER_ERROR_INVALID_ARGUMENT;
    std::lock_guard guard(lock);
    if (!owners.count(owner_handle)) return SCE_AGC_DRIVER_ERROR_NOT_FOUND;
    *resource_handle = nextHandle++;
    resources.emplace(*resource_handle, Resource{owner_handle, memory, size, name ? name : "", type});
    return 0;
}

int APS5_VABI sceAgcDriverRegisterWorkloadStream(uint32_t stream_id, const void* stream) {
 (void)stream_id;
 (void)stream;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceAgcDriverUnregisterOwnerAndResources(uint32_t owner_handle) {
    std::lock_guard guard(lock);
    if (owners.erase(owner_handle) == 0) return SCE_AGC_DRIVER_ERROR_NOT_FOUND;
    std::erase_if(resources, [owner_handle](const auto& entry) { return entry.second.owner == owner_handle; });
    return 0;
}

int APS5_VABI sceAgcDriverUnregisterResource(uint32_t resource_handle) {
    std::lock_guard guard(lock);
    return resources.erase(resource_handle) == 1 ? 0 : SCE_AGC_DRIVER_ERROR_NOT_FOUND;
}

}
