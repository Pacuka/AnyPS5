#include <cstddef>
#include <cstring>
#include <cstdint>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libSceUserService/UserService.hpp"

extern "C" {

int APS5_VABI sceUserServiceGetAccessibilityChatTranscription(int user_id, int32_t* chat_transcription) {
 if (user_id != USER_SERVICE_INITIAL_USER_ID || chat_transcription == nullptr) return USER_SERVICE_ERROR_INVALID_ARGUMENT;
 // Accessibility options are left at their system defaults (disabled).
 *chat_transcription = 0;
 return USER_SERVICE_OK;
}

int APS5_VABI sceUserServiceGetAccessibilityPressAndHoldDelay(int user_id, int32_t* press_and_hold_delay) {
 if (user_id != USER_SERVICE_INITIAL_USER_ID || press_and_hold_delay == nullptr) return USER_SERVICE_ERROR_INVALID_ARGUMENT;
 // Accessibility options are left at their system defaults (disabled).
 *press_and_hold_delay = 0;
 return USER_SERVICE_OK;
}

int APS5_VABI sceUserServiceGetAccessibilityTriggerEffect(int user_id, int32_t* trigger_effect) {
 if (user_id != USER_SERVICE_INITIAL_USER_ID || trigger_effect == nullptr) return USER_SERVICE_ERROR_INVALID_ARGUMENT;
 // Accessibility options are left at their system defaults (disabled).
 *trigger_effect = 0;
 return USER_SERVICE_OK;
}

int APS5_VABI sceUserServiceGetAccessibilityVibration(int user_id, int32_t* vibration) {
 if (user_id != USER_SERVICE_INITIAL_USER_ID || vibration == nullptr) return USER_SERVICE_ERROR_INVALID_ARGUMENT;
 // Accessibility options are left at their system defaults (disabled).
 *vibration = 0;
 return USER_SERVICE_OK;
}

int APS5_VABI sceUserServiceGetAccessibilityZoomEnabled(int user_id, int32_t* zoom_enabled) {
 if (user_id != USER_SERVICE_INITIAL_USER_ID || zoom_enabled == nullptr) return USER_SERVICE_ERROR_INVALID_ARGUMENT;
 // Accessibility options are left at their system defaults (disabled).
 *zoom_enabled = 0;
 return USER_SERVICE_OK;
}

int APS5_VABI sceUserServiceGetAgeLevel(int user_id, uint32_t* age_level) {
 (void)user_id;
 (void)age_level;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceUserServiceGetEvent(SceUserServiceEvent* event) {
 if (event == nullptr) return USER_SERVICE_ERROR_INVALID_ARGUMENT;
 // The single local user is reported as logged in once; there are no further account changes.
 static bool loginReported = false;
 if (loginReported) return USER_SERVICE_ERROR_NO_EVENT;
 loginReported = true;
 event->event_type = USER_SERVICE_EVENT_TYPE_LOGIN;
 event->user_id = USER_SERVICE_INITIAL_USER_ID;
 return USER_SERVICE_OK;
}

int APS5_VABI sceUserServiceGetGamePresets(int user_id, UserServiceGamePresets* presets) {
 // System-wide game presets (difficulty, view inversion, subtitles, audio language) of the user's
 // profile. A fresh console has none set: every field is 0, "not specified", and the title falls
 // back to its own defaults. this_size is the caller's.
 if (presets == nullptr || user_id != USER_SERVICE_INITIAL_USER_ID) {
  return USER_SERVICE_ERROR_INVALID_ARGUMENT;
 }
 presets->difficulty = 0;
 presets->priority = 0;
 presets->invert_vertical_view_for_1st_person_view = 0;
 presets->invert_horizontal_view_for_1st_person_view = 0;
 presets->invert_vertical_view_for_3rd_person_view = 0;
 presets->invert_horizontal_view_for_3rd_person_view = 0;
 presets->display_sub_titles = 0;
 presets->audio_language = 0;
 return USER_SERVICE_OK;
}

int APS5_VABI sceUserServiceGetInitialUser(int* user_id) {
 if (user_id == nullptr) {
  return USER_SERVICE_ERROR_INVALID_ARGUMENT;
 }
 *user_id = USER_SERVICE_INITIAL_USER_ID;
 return USER_SERVICE_OK;
}

int APS5_VABI sceUserServiceGetLoginUserIdList(UserServiceLoginUserIdList* user_id_list) {
 if (user_id_list == nullptr) {
  return USER_SERVICE_ERROR_INVALID_ARGUMENT;
 }
 user_id_list->user_id[0] = USER_SERVICE_INITIAL_USER_ID;
 user_id_list->user_id[1] = USER_SERVICE_USER_ID_INVALID;
 user_id_list->user_id[2] = USER_SERVICE_USER_ID_INVALID;
 user_id_list->user_id[3] = USER_SERVICE_USER_ID_INVALID;
 return USER_SERVICE_OK;
}

int APS5_VABI sceUserServiceGetUserName(int user_id, char* name, size_t size) {
 if (user_id != USER_SERVICE_INITIAL_USER_ID || name == nullptr) return USER_SERVICE_ERROR_INVALID_ARGUMENT;
 const size_t length = std::strlen(USER_SERVICE_INITIAL_USER_NAME);
 if (size <= length) return USER_SERVICE_ERROR_BUFFER_TOO_SHORT;
 std::memcpy(name, USER_SERVICE_INITIAL_USER_NAME, length + 1);
 return USER_SERVICE_OK;
}

int APS5_VABI sceUserServiceGetUserNumber(int user_id, int32_t* number) {
 if (user_id != USER_SERVICE_INITIAL_USER_ID || number == nullptr) return USER_SERVICE_ERROR_INVALID_ARGUMENT;
 *number = 1;
 return USER_SERVICE_OK;
}

int APS5_VABI sceUserServiceInitialize(const void* params) {
 (void)params;
 return USER_SERVICE_OK;
}

int APS5_VABI sceUserServiceInitialize2(void) {
 return USER_SERVICE_OK;
}

int APS5_VABI sceUserServiceTerminate(void) {
 return USER_SERVICE_OK;
}


// No PSN account exists, so the platform privacy setting reports the feature as not permitted.
int APS5_VABI sceUserServiceGetPlatformPrivacyWs1(int32_t user_id, int32_t* value) {
    (void)user_id;
    if (!value) return static_cast<int>(0x80960002);
    *value = 0;
    return 0;
}
}
