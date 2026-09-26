#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_EQ_INCLUDE_EVENT_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_EQ_INCLUDE_EVENT_HPP

#include <cstdint>

namespace AgcDriver::Eq {

// Delivers a GPU end-of-pipe interrupt to every event queue registered through
// sceAgcDriverAddEqEvent; the context id is reported through sceAgcDriverGetEqContextId.
void TriggerEndOfPipe(std::uint32_t contextId);

}

#endif
