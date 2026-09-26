#include <cstdint>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <map>
#include <mutex>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

// The user is signed out of PSN: contexts, handles, filters and callbacks are local objects and
// work, while every request to the server fails with SCE_NP_ERROR_SIGNED_OUT.

namespace {

constexpr int SCE_NP_ERROR_INVALID_ARGUMENT = static_cast<int>(0x80550003);
constexpr int SCE_NP_ERROR_SIGNED_OUT = static_cast<int>(0x80550006);
constexpr int SCE_NP_WEBAPI2_ERROR_INVALID_ID = static_cast<int>(0x80553401);

enum class Kind { Library, UserContext, Request, PushHandle, PushFilter, PushCallback };

struct PushContextId {
    char uuid[37];
    char padding[3];
};

std::mutex lock;
std::map<std::int64_t, Kind> objects;
std::int64_t nextId = 1;
unsigned pushContextCounter = 0;

std::int64_t create(Kind kind) {
    std::lock_guard guard(lock);
    const std::int64_t id = nextId++;
    objects.emplace(id, kind);
    return id;
}

bool exists(std::int64_t id, Kind kind) {
    std::lock_guard guard(lock);
    const auto found = objects.find(id);
    return found != objects.end() && found->second == kind;
}

int destroy(std::int64_t id, Kind kind) {
    std::lock_guard guard(lock);
    const auto found = objects.find(id);
    if (found == objects.end() || found->second != kind) return SCE_NP_WEBAPI2_ERROR_INVALID_ID;
    objects.erase(found);
    return 0;
}

int check(std::int64_t id, Kind kind) {
    return exists(id, kind) ? 0 : SCE_NP_WEBAPI2_ERROR_INVALID_ID;
}

}

extern "C" {

int APS5_VABI sceNpWebApi2Initialize(int lib_http_ctx_id, size_t pool_size) {
    (void)lib_http_ctx_id;
    if (pool_size == 0) return SCE_NP_ERROR_INVALID_ARGUMENT;
    return static_cast<int>(create(Kind::Library));
}

int APS5_VABI sceNpWebApi2Terminate(int lib_ctx_id) {
    return destroy(lib_ctx_id, Kind::Library);
}

void APS5_VABI sceNpWebApi2CheckTimeout(void) {}

int APS5_VABI sceNpWebApi2CreateUserContext(int lib_ctx_id, int user_id) {
    (void)user_id;
    if (const int result = check(lib_ctx_id, Kind::Library); result != 0) return result;
    return static_cast<int>(create(Kind::UserContext));
}

int APS5_VABI sceNpWebApi2DeleteUserContext(int user_context_id) {
    return destroy(user_context_id, Kind::UserContext);
}

int APS5_VABI sceNpWebApi2CreateRequest(int user_context_id, const char* api_group, const char* path, const char* method, const void* content_parameter, int64_t* request_id) {
    (void)content_parameter;
    if (!api_group || !path || !method || !request_id) return SCE_NP_ERROR_INVALID_ARGUMENT;
    if (const int result = check(user_context_id, Kind::UserContext); result != 0) return result;
    *request_id = create(Kind::Request);
    return 0;
}

int APS5_VABI sceNpWebApi2DeleteRequest(int64_t request_id) {
    return destroy(request_id, Kind::Request);
}

int APS5_VABI sceNpWebApi2AbortRequest(int64_t request_id) {
    return check(request_id, Kind::Request);
}

int APS5_VABI sceNpWebApi2AddHttpRequestHeader(int64_t request_id, const char* field_name, const char* value) {
    if (!field_name || !value) return SCE_NP_ERROR_INVALID_ARGUMENT;
    return check(request_id, Kind::Request);
}

int APS5_VABI sceNpWebApi2SetRequestTimeout_nid_postfix(int64_t request_id, uint32_t timeout) {
    (void)timeout;
    return check(request_id, Kind::Request);
}

int APS5_VABI sceNpWebApi2SendRequest(int64_t request_id, const void* data, size_t data_size, NpWebApi2ResponseInformationOption* response_info_option) {
    (void)data;
    (void)data_size;
    (void)response_info_option;
    if (const int result = check(request_id, Kind::Request); result != 0) return result;
    return SCE_NP_ERROR_SIGNED_OUT;
}

int APS5_VABI sceNpWebApi2ReadData(int64_t request_id, void* data, size_t size) {
    if (!data && size != 0) return SCE_NP_ERROR_INVALID_ARGUMENT;
    if (const int result = check(request_id, Kind::Request); result != 0) return result;
    return SCE_NP_ERROR_SIGNED_OUT;
}

int APS5_VABI sceNpWebApi2GetHttpResponseHeaderValue(int64_t request_id, const char* field_name, char* value, size_t value_size) {
    (void)value_size;
    if (!field_name || !value) return SCE_NP_ERROR_INVALID_ARGUMENT;
    if (const int result = check(request_id, Kind::Request); result != 0) return result;
    return SCE_NP_ERROR_SIGNED_OUT;
}

int APS5_VABI sceNpWebApi2GetHttpResponseHeaderValueLength(int64_t request_id, const char* field_name, size_t* value_length) {
    if (!field_name || !value_length) return SCE_NP_ERROR_INVALID_ARGUMENT;
    if (const int result = check(request_id, Kind::Request); result != 0) return result;
    return SCE_NP_ERROR_SIGNED_OUT;
}

int APS5_VABI sceNpWebApi2PushEventCreateHandle(int lib_ctx_id) {
    if (const int result = check(lib_ctx_id, Kind::Library); result != 0) return result;
    return static_cast<int>(create(Kind::PushHandle));
}

int APS5_VABI sceNpWebApi2PushEventDeleteHandle(int lib_ctx_id, int handle_id) {
    if (const int result = check(lib_ctx_id, Kind::Library); result != 0) return result;
    return destroy(handle_id, Kind::PushHandle);
}

int APS5_VABI sceNpWebApi2PushEventCreateFilter(int lib_ctx_id, int handle_id, const char* np_service_name, uint32_t np_service_label, const void* filter_param, size_t filter_param_num) {
    (void)np_service_name;
    (void)np_service_label;
    (void)filter_param;
    (void)filter_param_num;
    if (const int result = check(lib_ctx_id, Kind::Library); result != 0) return result;
    if (const int result = check(handle_id, Kind::PushHandle); result != 0) return result;
    return static_cast<int>(create(Kind::PushFilter));
}

int APS5_VABI sceNpWebApi2PushEventDeleteFilter_nid_postfix(int lib_ctx_id, int filter_id) {
    if (const int result = check(lib_ctx_id, Kind::Library); result != 0) return result;
    return destroy(filter_id, Kind::PushFilter);
}

int APS5_VABI sceNpWebApi2PushEventCreatePushContext_nid_postfix(int user_context_id, PushContextId* push_context_id) {
    if (!push_context_id) return SCE_NP_ERROR_INVALID_ARGUMENT;
    if (const int result = check(user_context_id, Kind::UserContext); result != 0) return result;
    std::lock_guard guard(lock);
    std::memset(push_context_id, 0, sizeof(*push_context_id));
    std::snprintf(push_context_id->uuid, sizeof(push_context_id->uuid), "00000000-0000-4000-8000-%012x", ++pushContextCounter);
    return 0;
}

int APS5_VABI sceNpWebApi2PushEventDeletePushContext(int user_context_id, const void* push_context_id) {
    if (!push_context_id) return SCE_NP_ERROR_INVALID_ARGUMENT;
    return check(user_context_id, Kind::UserContext);
}

// Push events come from the PSN server, so registered callbacks are never invoked while offline.
int APS5_VABI sceNpWebApi2PushEventRegisterCallback(int user_context_id, int filter_id, void* callback, void* user_arg) {
    (void)user_arg;
    if (!callback) return SCE_NP_ERROR_INVALID_ARGUMENT;
    if (const int result = check(user_context_id, Kind::UserContext); result != 0) return result;
    if (const int result = check(filter_id, Kind::PushFilter); result != 0) return result;
    return static_cast<int>(create(Kind::PushCallback));
}

int APS5_VABI sceNpWebApi2PushEventRegisterPushContextCallback_nid_postfix(int user_context_id, int filter_id, void* callback, void* user_arg) {
    return sceNpWebApi2PushEventRegisterCallback(user_context_id, filter_id, callback, user_arg);
}

int APS5_VABI sceNpWebApi2PushEventStartPushContextCallback_nid_postfix(int user_context_id, const PushContextId* push_context_id) {
    if (!push_context_id) return SCE_NP_ERROR_INVALID_ARGUMENT;
    return check(user_context_id, Kind::UserContext);
}

int APS5_VABI sceNpWebApi2PushEventUnregisterCallback_nid_postfix(int user_context_id, int callback_id) {
    if (const int result = check(user_context_id, Kind::UserContext); result != 0) return result;
    return destroy(callback_id, Kind::PushCallback);
}

int APS5_VABI sceNpWebApi2PushEventUnregisterPushContextCallback_nid_postfix(int user_context_id, int callback_id) {
    return sceNpWebApi2PushEventUnregisterCallback_nid_postfix(user_context_id, callback_id);
}

}
