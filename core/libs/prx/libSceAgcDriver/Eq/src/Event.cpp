#include "prx/libSceAgcDriver/Eq/include/Event.hpp"

#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libkernel/Equeue/Equeue.hpp"
#include <algorithm>
#include <mutex>
#include <utility>
#include <vector>

namespace {

// GPU completion events (end-of-pipe interrupts) use the graphics-core filter.
constexpr int16_t EVFILT_GRAPHICS_CORE = -14;

std::mutex registrationMutex;
std::vector<std::pair<KernelEqueue, int>> registrations;

}

namespace AgcDriver::Eq {

void TriggerEndOfPipe(std::uint32_t contextId) {
    std::lock_guard lock(registrationMutex);
    for (const auto& [eq, id] : registrations) {
        EqueueTriggerEvent_nid_postfix(eq, static_cast<uintptr_t>(id), EVFILT_GRAPHICS_CORE, reinterpret_cast<void*>(static_cast<uintptr_t>(contextId)));
    }
}

}

extern "C" {

int APS5_VABI sceAgcDriverAddEqEvent(KernelEqueue eq, int id, void* udata) {
    KernelEqueueEvent event{};
    event.event.ident = static_cast<uintptr_t>(id);
    event.event.filter = EVFILT_GRAPHICS_CORE;
    event.event.flags = EV_ADD | EV_CLEAR;
    event.event.udata = udata;
    event.filter.triggerFunc = [](KernelEqueueEvent* e, void* data) {
        KernelEvent triggered = e->event;
        triggered.data = static_cast<intptr_t>(reinterpret_cast<uintptr_t>(data));
        if (e->triggered) {
            e->pendingEvents.push_back(triggered);
        } else {
            e->event = triggered;
            e->triggered = true;
        }
    };
    event.filter.resetFunc = [](KernelEqueueEvent* e) {
        e->triggered = false;
        e->event.fflags = 0;
        e->event.data = 0;
    };
    const auto result = EqueueAddEvent_nid_postfix(eq, event);
    if (result == 0) {
        std::lock_guard lock(registrationMutex);
        if (std::find(registrations.begin(), registrations.end(), std::pair{eq, id}) == registrations.end()) registrations.emplace_back(eq, id);
    }
    return result;
}

int APS5_VABI sceAgcDriverDeleteEqEvent(KernelEqueue eq, int id) {
    {
        std::lock_guard lock(registrationMutex);
        std::erase(registrations, std::pair{eq, id});
    }
    return EqueueDeleteEvent_nid_postfix(eq, static_cast<uintptr_t>(id), EVFILT_GRAPHICS_CORE);
}

}
