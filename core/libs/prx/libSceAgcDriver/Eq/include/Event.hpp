#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_EQ_INCLUDE_EVENT_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_EQ_INCLUDE_EVENT_HPP

#include <cstdint>

namespace AgcDriver::Eq {

// Delivers a GPU end-of-pipe interrupt raised by a queue (0 for graphics, 0x20-0x57 for compute) to
// the event queues registered for that queue id through sceAgcDriverAddEqEvent; the context id is
// reported through sceAgcDriverGetEqContextId.
void TriggerEndOfPipe(std::uint32_t queue, std::uint32_t contextId);

}

#endif
