#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <vector>
#ifndef _WIN32
#include <unistd.h>
#endif
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libSceSystemService/SystemService.hpp"

namespace {

constexpr int SYSTEM_SERVICE_ERROR_INTERNAL = static_cast<int>(0x80A10001);
constexpr int SYSTEM_SERVICE_ERROR_UNAVAILABLE = static_cast<int>(0x80A10002);
constexpr int SYSTEM_SERVICE_PARAM_ID_SYSTEM_NAME = 6;
constexpr const char* SYSTEM_NAME = "PS5";
// The guest reserves 0x40 bytes for SceSystemServicePlayerDialogParam.
constexpr size_t PLAYER_DIALOG_PARAM_SIZE = 0x40;

bool noticeScreenSkipFlag = false;

}

extern "C" {

int APS5_VABI sceSystemServiceDisableNoticeScreenSkipFlagAutoSet(void) {
 return SYSTEM_SERVICE_OK;
}

int APS5_VABI sceSystemServiceGetDisplaySafeAreaInfo(SystemServiceDisplaySafeAreaInfo* info) {
 if (info == nullptr) {
  return SYSTEM_SERVICE_ERROR_PARAMETER;
 }
 *info = SystemServiceDisplaySafeAreaInfo{};
 // The whole host window is visible, so the safe area covers the full frame.
 info->ratio = 1.0f;
 return SYSTEM_SERVICE_OK;
}

int APS5_VABI sceSystemServiceGetHdrToneMapLuminance(SystemServiceHdrToneMapLuminance* luminance) {
 if (luminance == nullptr) {
  return SYSTEM_SERVICE_ERROR_PARAMETER;
 }
 luminance->max_full_frame_tone_map_luminance = 1000.0f;
 luminance->max_tone_map_luminance = 1000.0f;
 luminance->min_tone_map_luminance = 0.0f;
 return SYSTEM_SERVICE_OK;
}

int APS5_VABI sceSystemServiceGetNoticeScreenSkipFlag(bool* value) {
 if (value == nullptr) {
  return SYSTEM_SERVICE_ERROR_PARAMETER;
 }
 *value = noticeScreenSkipFlag;
 return SYSTEM_SERVICE_OK;
}

int APS5_VABI sceSystemServiceGetStatus(SystemServiceStatus* status) {
 if (status == nullptr) {
  return SYSTEM_SERVICE_ERROR_PARAMETER;
 }
 *status = SystemServiceStatus{};
 return SYSTEM_SERVICE_OK;
}

int APS5_VABI sceSystemServiceHideSplashScreen(void) {
 return SYSTEM_SERVICE_OK;
}

int APS5_VABI sceSystemServiceParamGetInt(int paramId, int* value) {
 if (value == nullptr) {
  return SYSTEM_SERVICE_ERROR_PARAMETER;
 }
 switch (paramId) {
  case SYSTEM_SERVICE_PARAM_ID_LANG: *value = SYSTEM_SERVICE_PARAM_LANG_ENGLISH_US; break;
  case SYSTEM_SERVICE_PARAM_ID_DATE_FORMAT: *value = SYSTEM_SERVICE_PARAM_DATE_FORMAT_DDMMYYYY; break;
  case SYSTEM_SERVICE_PARAM_ID_TIME_FORMAT: *value = SYSTEM_SERVICE_PARAM_TIME_FORMAT_24HOUR; break;
  case SYSTEM_SERVICE_PARAM_ID_TIME_ZONE: *value = 0; break;
  case SYSTEM_SERVICE_PARAM_ID_SUMMERTIME: *value = 0; break;
  case SYSTEM_SERVICE_PARAM_ID_GAME_PARENTAL_LEVEL: *value = SYSTEM_SERVICE_PARAM_GAME_PARENTAL_OFF; break;
  case SYSTEM_SERVICE_PARAM_ID_ENTER_BUTTON_ASSIGN: *value = SYSTEM_SERVICE_PARAM_ENTER_BUTTON_CROSS; break;
  default: *value = 0; break;
 }
 return SYSTEM_SERVICE_OK;
}

int APS5_VABI sceSystemServiceParamGetString(int param_id, char* buf, size_t buf_size) {
 if (buf == nullptr || param_id != SYSTEM_SERVICE_PARAM_ID_SYSTEM_NAME) {
  return SYSTEM_SERVICE_ERROR_PARAMETER;
 }
 const size_t length = std::strlen(SYSTEM_NAME);
 if (buf_size <= length) {
  return SYSTEM_SERVICE_ERROR_PARAMETER;
 }
 std::memcpy(buf, SYSTEM_NAME, length + 1);
 return SYSTEM_SERVICE_OK;
}

int APS5_VABI sceSystemServicePowerTick(void) {
 return SYSTEM_SERVICE_OK;
}

int APS5_VABI sceSystemServiceReceiveEvent(SystemServiceEvent* event) {
 if (event == nullptr) {
  return SYSTEM_SERVICE_ERROR_PARAMETER;
 }
 event->event_type = -1;
 std::memset(event->data, 0, sizeof(event->data));
 return SYSTEM_SERVICE_ERROR_NO_EVENT;
}

int APS5_VABI sceSystemServiceReportAbnormalTermination(const void* info) {
 (void)info;
 return SYSTEM_SERVICE_OK;
}

int APS5_VABI sceSystemServiceSetNoticeScreenSkipFlag(void) {
 noticeScreenSkipFlag = true;
 return SYSTEM_SERVICE_OK;
}

}

extern "C" {

int APS5_VABI sceSystemServiceInitializePlayerDialogParam_nid_postfix(void* param) {
 if (param == nullptr) {
  return SYSTEM_SERVICE_ERROR_PARAMETER;
 }
 std::memset(param, 0, PLAYER_DIALOG_PARAM_SIZE);
 return SYSTEM_SERVICE_OK;
}

int APS5_VABI sceSystemServiceLaunchPlayerDialog_nid_postfix(const void* param) {
 if (param == nullptr) {
  return SYSTEM_SERVICE_ERROR_PARAMETER;
 }
 // Player profiles come from PSN, which is unreachable.
 return SYSTEM_SERVICE_ERROR_UNAVAILABLE;
}

int APS5_VABI sceSystemServiceLoadExec_nid_postfix(const char* path, char* const argv[]) {
 (void)path;
#ifdef _WIN32
 (void)argv;
 throw std::runtime_error("sceSystemServiceLoadExec is not implemented on Windows yet");
#else
 // Restarts the application: the host executable is this relinked game.
 std::vector<char*> arguments{const_cast<char*>("/proc/self/exe")};
 for (char* const* cursor = argv; cursor != nullptr && *cursor != nullptr; ++cursor) arguments.push_back(*cursor);
 arguments.push_back(nullptr);
 std::fflush(nullptr);
 ::execv("/proc/self/exe", arguments.data());
 return SYSTEM_SERVICE_ERROR_INTERNAL;
#endif
}

}
