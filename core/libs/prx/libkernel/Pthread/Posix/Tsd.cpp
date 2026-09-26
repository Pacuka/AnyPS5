#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"
#include "../include/Pthread.hpp"

using namespace PthreadSync;

extern "C" {

void* APS5_VABI pthread_getspecific_nid_postfix(PthreadKey key) {
    return GetSpecific_nid_no_patch(key);
}

int APS5_VABI pthread_setspecific_nid_postfix(PthreadKey key, void* value) {
    return SetSpecific_nid_no_patch(key, value);
}

int APS5_VABI pthread_key_create_nid_postfix(PthreadKey* key, pthread_key_destructor_func_t destructor) {
    return KeyCreate_nid_no_patch(key, destructor);
}

int APS5_VABI pthread_key_delete_nid_postfix(PthreadKey key) {
    return KeyDelete_nid_no_patch(key);
}

int APS5_VABI pthread_once_nid_postfix(int* once, void (APS5_VABI *routine)()) {
    return Once_nid_no_patch(once, routine);
}

}
