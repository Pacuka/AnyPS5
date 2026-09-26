#include <atomic>
#include <cstdint>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

namespace {

constexpr int ERROR_DIALOG_STATUS_NONE = 0;
constexpr int ERROR_DIALOG_STATUS_INITIALIZED = 1;
constexpr int ERROR_DIALOG_STATUS_FINISHED = 3;

constexpr int ERROR_DIALOG_ERROR_NOT_INITIALIZED = static_cast<int>(0x80ed0001u);
constexpr int ERROR_DIALOG_ERROR_ALREADY_INITIALIZED = static_cast<int>(0x80ed0002u);
constexpr int ERROR_DIALOG_ERROR_PARAM_INVALID = static_cast<int>(0x80ed0003u);

struct ErrorDialogParam {
    std::size_t size;
    std::int32_t errorCode;
    std::int32_t userId;
    std::int32_t reserved;
};

std::atomic<int> g_status{ERROR_DIALOG_STATUS_NONE};

}

extern "C" {

int APS5_VABI sceErrorDialogInitialize(void) {
    int expected = ERROR_DIALOG_STATUS_NONE;
    if (!g_status.compare_exchange_strong(expected, ERROR_DIALOG_STATUS_INITIALIZED)) return ERROR_DIALOG_ERROR_ALREADY_INITIALIZED;
    return 0;
}

// There is no system dialog to show: the error is reported on the console and the dialog finishes
// at once, as if the user had acknowledged it.
int APS5_VABI sceErrorDialogOpen(const void* param) {
    if (g_status.load() == ERROR_DIALOG_STATUS_NONE) return ERROR_DIALOG_ERROR_NOT_INITIALIZED;
    if (param == nullptr) return ERROR_DIALOG_ERROR_PARAM_INVALID;
    ErrorDialogParam dialog{};
    std::memcpy(&dialog, param, sizeof(dialog));
    std::fprintf(stderr, "[ErrorDialog] error 0x%08x for user %d\n", static_cast<std::uint32_t>(dialog.errorCode), dialog.userId);
    g_status.store(ERROR_DIALOG_STATUS_FINISHED);
    return 0;
}

int APS5_VABI sceErrorDialogUpdateStatus(void) {
    return g_status.load();
}

int APS5_VABI sceErrorDialogGetStatus(void) {
    return g_status.load();
}

int APS5_VABI sceErrorDialogClose(void) {
    if (g_status.load() == ERROR_DIALOG_STATUS_NONE) return ERROR_DIALOG_ERROR_NOT_INITIALIZED;
    g_status.store(ERROR_DIALOG_STATUS_FINISHED);
    return 0;
}

int APS5_VABI sceErrorDialogTerminate(void) {
    if (g_status.exchange(ERROR_DIALOG_STATUS_NONE) == ERROR_DIALOG_STATUS_NONE) return ERROR_DIALOG_ERROR_NOT_INITIALIZED;
    return 0;
}

}
