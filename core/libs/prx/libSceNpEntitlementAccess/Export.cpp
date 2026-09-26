#include <cstdint>
#include <cstddef>
#include <mutex>
#include <set>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

// No additional content or service entitlements are installed, so every entitlement query
// completes immediately with an empty result.

namespace {

constexpr int SCE_NP_ENTITLEMENT_ACCESS_ERROR_PARAMETER = static_cast<int>(0x80558802);
constexpr int SCE_NP_ENTITLEMENT_ACCESS_ERROR_NO_ENTITLEMENT = static_cast<int>(0x80558808);
constexpr int SCE_NP_ENTITLEMENT_ACCESS_ERROR_INVALID_REQUEST = static_cast<int>(0x8055880A);
constexpr std::uint32_t SKU_FLAG_FULL = 3;

std::mutex lock;
std::set<std::int64_t> requests;
std::int64_t nextRequest = 1;

int createRequest(std::int64_t* request_id) {
    if (!request_id) return SCE_NP_ENTITLEMENT_ACCESS_ERROR_PARAMETER;
    std::lock_guard guard(lock);
    *request_id = nextRequest++;
    requests.insert(*request_id);
    return 0;
}

// Poll(request, state, list, listNum, hitNum, nextOffset, previousOffset): the request is
// already complete and matched nothing.
int pollRequest(std::int64_t request_id, std::int32_t* state, std::uint32_t* hit_num, std::uint32_t* next_offset, std::uint32_t* previous_offset) {
    {
        std::lock_guard guard(lock);
        if (!requests.count(request_id)) return SCE_NP_ENTITLEMENT_ACCESS_ERROR_INVALID_REQUEST;
    }
    if (state) *state = 0;
    if (hit_num) *hit_num = 0;
    if (next_offset) *next_offset = 0;
    if (previous_offset) *previous_offset = 0;
    return 0;
}

}

extern "C" {

int APS5_VABI sceNpEntitlementAccessInitialize(const NpEntitlementAccessInitParam* init_param, NpEntitlementAccessBootParam* boot_param) {
    (void)init_param;
    (void)boot_param;
    return 0;
}

int APS5_VABI sceNpEntitlementAccessGetSkuFlag(uint32_t* sku_flag) {
    if (!sku_flag) return SCE_NP_ENTITLEMENT_ACCESS_ERROR_PARAMETER;
    *sku_flag = SKU_FLAG_FULL;
    return 0;
}

int APS5_VABI sceNpEntitlementAccessGetAddcontEntitlementInfo(uint32_t service_label, const NpUnifiedEntitlementLabel* entitlement_label, NpEntitlementAccessAddcontEntitlementInfo* info) {
    (void)service_label;
    if (!entitlement_label || !info) return SCE_NP_ENTITLEMENT_ACCESS_ERROR_PARAMETER;
    return SCE_NP_ENTITLEMENT_ACCESS_ERROR_NO_ENTITLEMENT;
}

int APS5_VABI sceNpEntitlementAccessGetAddcontEntitlementInfoList(uint32_t service_label, NpEntitlementAccessAddcontEntitlementInfo* list, uint32_t list_num, uint32_t* hit_num) {
    (void)service_label;
    (void)list;
    (void)list_num;
    if (!hit_num) return SCE_NP_ENTITLEMENT_ACCESS_ERROR_PARAMETER;
    *hit_num = 0;
    return 0;
}

int APS5_VABI sceNpEntitlementAccessRequestUnifiedEntitlementInfoList_nid_postfix(uint32_t service_label, int user_id, uint32_t package_type, uint32_t offset, const void* param, std::int64_t* request_id) {
    (void)service_label;
    (void)user_id;
    (void)package_type;
    (void)offset;
    (void)param;
    return createRequest(request_id);
}

int APS5_VABI sceNpEntitlementAccessRequestServiceEntitlementInfoList_nid_postfix(uint32_t service_label, int user_id, uint32_t package_type, uint32_t offset, const void* param, std::int64_t* request_id) {
    return sceNpEntitlementAccessRequestUnifiedEntitlementInfoList_nid_postfix(service_label, user_id, package_type, offset, param, request_id);
}

int APS5_VABI sceNpEntitlementAccessPollUnifiedEntitlementInfoList_nid_postfix(std::int64_t request_id, std::int32_t* state, void* list, uint32_t list_num, uint32_t* hit_num, uint32_t* next_offset, uint32_t* previous_offset) {
    (void)list;
    (void)list_num;
    return pollRequest(request_id, state, hit_num, next_offset, previous_offset);
}

int APS5_VABI sceNpEntitlementAccessPollServiceEntitlementInfoList_nid_postfix(std::int64_t request_id, std::int32_t* state, void* list, uint32_t list_num, uint32_t* hit_num, uint32_t* next_offset, uint32_t* previous_offset) {
    (void)list;
    (void)list_num;
    return pollRequest(request_id, state, hit_num, next_offset, previous_offset);
}

int APS5_VABI sceNpEntitlementAccessDeleteRequest_nid_postfix(std::int64_t request_id) {
    std::lock_guard guard(lock);
    return requests.erase(request_id) == 1 ? 0 : SCE_NP_ENTITLEMENT_ACCESS_ERROR_INVALID_REQUEST;
}

}
