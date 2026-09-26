#include <cstdint>
#include <cstddef>
#include <cstring>
#include <mutex>
#include <set>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libSceUserService/UserService.hpp"

// The local user has a PSN account but is signed out, as on a console without a network:
// account identity is available, every server request fails with SCE_NP_ERROR_SIGNED_OUT and the
// sign-in state never changes, so registered callbacks are never invoked.

namespace {

constexpr int SCE_NP_ERROR_INVALID_ARGUMENT = static_cast<int>(0x80550003);
constexpr int SCE_NP_ERROR_SIGNED_OUT = static_cast<int>(0x80550006);
constexpr int SCE_NP_ERROR_USER_NOT_FOUND = static_cast<int>(0x80550007);
constexpr std::uint32_t SCE_NP_STATE_SIGNED_OUT = 1;
constexpr std::uint32_t SCE_NP_REACHABILITY_STATE_UNAVAILABLE = 0;
constexpr std::uint64_t LOCAL_ACCOUNT_ID = 0x0000000100000001ull;
constexpr const char* LOCAL_ONLINE_ID = "Player";
constexpr int STATE_CALLBACK_ID = 1;

std::mutex lock;
std::set<int> requests;
int nextRequest = 1;

bool isLocalUser(int user_id) {
    return user_id == USER_SERVICE_INITIAL_USER_ID;
}

int createRequest() {
    std::lock_guard guard(lock);
    const int id = nextRequest++;
    requests.insert(id);
    return id;
}

bool isRequest(int req_id) {
    std::lock_guard guard(lock);
    return requests.count(req_id) != 0;
}

// Server queries need a valid request and the local user, and then fail because it is signed out.
int serverQuery(int req_id, int user_id) {
    if (!isRequest(req_id)) return SCE_NP_ERROR_INVALID_ARGUMENT;
    if (!isLocalUser(user_id)) return SCE_NP_ERROR_USER_NOT_FOUND;
    return SCE_NP_ERROR_SIGNED_OUT;
}

void fillOnlineId(NpOnlineId* online_id) {
    std::memset(online_id, 0, sizeof(*online_id));
    std::strncpy(online_id->data, LOCAL_ONLINE_ID, sizeof(online_id->data) - 1);
}

}

extern "C" {

int APS5_VABI sceNpSetNpTitleId(const NpTitleId* title_id, const NpTitleSecret* title_secret) {
    return title_id && title_secret ? 0 : SCE_NP_ERROR_INVALID_ARGUMENT;
}

int APS5_VABI sceNpSetContentRestriction(const NpContentRestriction* restriction) {
    return restriction ? 0 : SCE_NP_ERROR_INVALID_ARGUMENT;
}

int APS5_VABI sceNpCreateRequest(void) {
    return createRequest();
}

int APS5_VABI sceNpCreateAsyncRequest(const NpCreateAsyncRequestParameter* param) {
    if (!param) return SCE_NP_ERROR_INVALID_ARGUMENT;
    return createRequest();
}

int APS5_VABI sceNpDeleteRequest(int req_id) {
    std::lock_guard guard(lock);
    return requests.erase(req_id) == 1 ? 0 : SCE_NP_ERROR_INVALID_ARGUMENT;
}

int APS5_VABI sceNpAbortRequest(int req_id) {
    return isRequest(req_id) ? 0 : SCE_NP_ERROR_INVALID_ARGUMENT;
}

// Asynchronous operations fail when they are issued, so polling never has pending work.
int APS5_VABI sceNpPollAsync(int req_id, int* result) {
    if (!result || !isRequest(req_id)) return SCE_NP_ERROR_INVALID_ARGUMENT;
    *result = SCE_NP_ERROR_SIGNED_OUT;
    return 0;
}

int APS5_VABI sceNpCheckCallback(void) {
    return 0;
}

int APS5_VABI sceNpGetState(int user_id, uint32_t* state) {
    if (!state) return SCE_NP_ERROR_INVALID_ARGUMENT;
    if (!isLocalUser(user_id)) return SCE_NP_ERROR_USER_NOT_FOUND;
    *state = SCE_NP_STATE_SIGNED_OUT;
    return 0;
}

int APS5_VABI sceNpHasSignedUp(int user_id, bool* has_signed_up) {
    if (!has_signed_up) return SCE_NP_ERROR_INVALID_ARGUMENT;
    if (!isLocalUser(user_id)) return SCE_NP_ERROR_USER_NOT_FOUND;
    *has_signed_up = true;
    return 0;
}

int APS5_VABI sceNpGetAccountIdA(int user_id, uint64_t* account_id) {
    if (!account_id) return SCE_NP_ERROR_INVALID_ARGUMENT;
    if (!isLocalUser(user_id)) return SCE_NP_ERROR_USER_NOT_FOUND;
    *account_id = LOCAL_ACCOUNT_ID;
    return 0;
}

int APS5_VABI sceNpGetUserIdByAccountId_nid_postfix(uint64_t account_id, int* user_id) {
    if (!user_id) return SCE_NP_ERROR_INVALID_ARGUMENT;
    if (account_id != LOCAL_ACCOUNT_ID) return SCE_NP_ERROR_USER_NOT_FOUND;
    *user_id = USER_SERVICE_INITIAL_USER_ID;
    return 0;
}

int APS5_VABI sceNpGetOnlineId(int user_id, NpOnlineId* online_id) {
    if (!online_id) return SCE_NP_ERROR_INVALID_ARGUMENT;
    if (!isLocalUser(user_id)) return SCE_NP_ERROR_USER_NOT_FOUND;
    fillOnlineId(online_id);
    return 0;
}

int APS5_VABI sceNpGetNpId(int user_id, NpId* np_id) {
    if (!np_id) return SCE_NP_ERROR_INVALID_ARGUMENT;
    if (!isLocalUser(user_id)) return SCE_NP_ERROR_USER_NOT_FOUND;
    std::memset(np_id, 0, sizeof(*np_id));
    fillOnlineId(&np_id->online_id);
    return 0;
}

int APS5_VABI sceNpGetAccountCountryA(int user_id, void* country_code) {
    if (!country_code) return SCE_NP_ERROR_INVALID_ARGUMENT;
    if (!isLocalUser(user_id)) return SCE_NP_ERROR_USER_NOT_FOUND;
    const char code[4] = {'u', 's', 0, 0};
    std::memcpy(country_code, code, sizeof(code));
    return 0;
}

int APS5_VABI sceNpGetNpReachabilityState(int user_id, uint32_t* state) {
    if (!state) return SCE_NP_ERROR_INVALID_ARGUMENT;
    if (!isLocalUser(user_id)) return SCE_NP_ERROR_USER_NOT_FOUND;
    *state = SCE_NP_REACHABILITY_STATE_UNAVAILABLE;
    return 0;
}

int APS5_VABI sceNpCheckNpReachability(int req_id, int user_id) {
    return serverQuery(req_id, user_id);
}

int APS5_VABI sceNpCheckNpAvailability(int req_id, const char* user, void* result) {
    (void)result;
    if (!user || !isRequest(req_id)) return SCE_NP_ERROR_INVALID_ARGUMENT;
    return SCE_NP_ERROR_SIGNED_OUT;
}

int APS5_VABI sceNpCheckPremium(int req_id, const NpCheckPremiumParameter* param, NpCheckPremiumResult* result) {
    if (!param || !result || !isRequest(req_id)) return SCE_NP_ERROR_INVALID_ARGUMENT;
    return SCE_NP_ERROR_SIGNED_OUT;
}

int APS5_VABI sceNpGetAccountAge(int req_id, int user_id, uint8_t* age) {
    if (!age) return SCE_NP_ERROR_INVALID_ARGUMENT;
    return serverQuery(req_id, user_id);
}

int APS5_VABI sceNpNotifyPremiumFeature_nid_postfix(const void* param) {
    return param ? 0 : SCE_NP_ERROR_INVALID_ARGUMENT;
}

int APS5_VABI sceNpRegisterStateCallback(void* callback, void* userdata) {
    (void)userdata;
    return callback ? STATE_CALLBACK_ID : SCE_NP_ERROR_INVALID_ARGUMENT;
}

int APS5_VABI sceNpRegisterStateCallbackA_nid_postfix(void* callback, void* userdata) {
    return sceNpRegisterStateCallback(callback, userdata);
}

int APS5_VABI sceNpUnregisterStateCallback(void) {
    return 0;
}

void APS5_VABI sceNpRegisterGamePresenceCallback(void* callback, void* userdata) {
    (void)callback;
    (void)userdata;
}

int APS5_VABI sceNpRegisterNpReachabilityStateCallback(void* callback, void* userdata) {
    (void)userdata;
    return callback ? 0 : SCE_NP_ERROR_INVALID_ARGUMENT;
}

int APS5_VABI sceNpRegisterPlusEventCallback(void* callback, void* userdata) {
    (void)userdata;
    return callback ? 0 : SCE_NP_ERROR_INVALID_ARGUMENT;
}

int APS5_VABI sceNpRegisterPremiumEventCallback(void* callback, void* userdata) {
    (void)userdata;
    return callback ? 0 : SCE_NP_ERROR_INVALID_ARGUMENT;
}


int APS5_VABI sceNpGetAccountLanguage2(int req_id, int user_id, void* language) {
    (void)req_id;
    (void)user_id;
    (void)language;
    return SCE_NP_ERROR_SIGNED_OUT;
}
int APS5_VABI sceNpUnregisterStateCallbackA(int callback_id) {
    (void)callback_id;
    return 0;
}
}
