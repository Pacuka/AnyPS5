#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

extern "C" {

int APS5_VABI __tls_get_addr_nid_postfix() {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceCoredumpSetUserDataType_nid_postfix() {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceCoredumpWriteUserData_nid_postfix() {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

// Reports whether the console runs in PS5 Pro ("Trinity") mode; games pick the Trinity GPU
// paths when it returns non-zero. The base PS5 is emulated.
int APS5_VABI sceKernelUnknownTU5e3f9gSiU_nid_no_patch() {
    return 0;
}

}

APS5_EXPORT("tU5e3f9gSiU", sceKernelUnknownTU5e3f9gSiU_nid_no_patch);
