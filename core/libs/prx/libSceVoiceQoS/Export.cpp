#include <cstdint>
#include <cstddef>
#include <algorithm>
#include <cstring>
#include <map>
#include <mutex>
#include <vector>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

// Voice chat endpoints are local bookkeeping. Without microphone capture or a network peer,
// outgoing packets are discarded and no incoming packets ever arrive.

namespace {

constexpr int SCE_VOICE_QOS_ERROR_NOT_INITIALIZED = static_cast<int>(0x80480001);
constexpr int SCE_VOICE_QOS_ERROR_INVALID_ARGUMENT = static_cast<int>(0x80480003);
constexpr int SCE_VOICE_QOS_ERROR_INVALID_ID = static_cast<int>(0x80480006);

constexpr int ATTRIBUTE_MIC_VOLUME = 0;
constexpr int ATTRIBUTE_SPEAKER_VOLUME = 2;
constexpr int ATTRIBUTE_SILENT_STATE = 6;

enum class Kind { Local, Remote, Connection };

struct Endpoint {
    Kind kind;
    std::map<int, std::vector<std::uint8_t>> attributes;
};

std::mutex lock;
bool initialized = false;
std::map<int, Endpoint> endpoints;
int nextId = 1;

int create(Kind kind, int* id) {
    if (!id) return SCE_VOICE_QOS_ERROR_INVALID_ARGUMENT;
    std::lock_guard guard(lock);
    if (!initialized) return SCE_VOICE_QOS_ERROR_NOT_INITIALIZED;
    *id = nextId++;
    endpoints.emplace(*id, Endpoint{kind, {}});
    return 0;
}

int destroy(int id, Kind kind) {
    std::lock_guard guard(lock);
    const auto found = endpoints.find(id);
    if (found == endpoints.end() || found->second.kind != kind) return SCE_VOICE_QOS_ERROR_INVALID_ID;
    endpoints.erase(found);
    return 0;
}

bool exists(int id, Kind kind) {
    std::lock_guard guard(lock);
    const auto found = endpoints.find(id);
    return found != endpoints.end() && found->second.kind == kind;
}

}

extern "C" {

int APS5_VABI sceVoiceQoSInit(void* mem_block, uint32_t mem_size, int32_t app_type) {
    (void)app_type;
    if (!mem_block || mem_size == 0) return SCE_VOICE_QOS_ERROR_INVALID_ARGUMENT;
    std::lock_guard guard(lock);
    initialized = true;
    return 0;
}

int APS5_VABI sceVoiceQoSEnd_nid_postfix(void) {
    std::lock_guard guard(lock);
    if (!initialized) return SCE_VOICE_QOS_ERROR_NOT_INITIALIZED;
    initialized = false;
    endpoints.clear();
    return 0;
}

int APS5_VABI sceVoiceQoSCreateLocalEndpoint_nid_postfix(int* local_id, int user_id, int32_t device_in_id, int32_t device_out_id) {
    (void)user_id;
    (void)device_in_id;
    (void)device_out_id;
    return create(Kind::Local, local_id);
}

int APS5_VABI sceVoiceQoSDeleteLocalEndpoint_nid_postfix(int local_id) {
    return destroy(local_id, Kind::Local);
}

int APS5_VABI sceVoiceQoSCreateRemoteEndpoint_nid_postfix(int* remote_id) {
    return create(Kind::Remote, remote_id);
}

int APS5_VABI sceVoiceQoSDeleteRemoteEndpoint_nid_postfix(int remote_id) {
    return destroy(remote_id, Kind::Remote);
}

int APS5_VABI sceVoiceQoSConnect_nid_postfix(int* connection_id, int local_id, int remote_id) {
    if (!exists(local_id, Kind::Local) || !exists(remote_id, Kind::Remote)) return SCE_VOICE_QOS_ERROR_INVALID_ID;
    return create(Kind::Connection, connection_id);
}

int APS5_VABI sceVoiceQoSDisconnect_nid_postfix(int connection_id) {
    return destroy(connection_id, Kind::Connection);
}

int APS5_VABI sceVoiceQoSSetLocalEndpointAttribute_nid_postfix(int local_id, int attribute_id, const void* value, int size) {
    if (!value || size <= 0) return SCE_VOICE_QOS_ERROR_INVALID_ARGUMENT;
    std::lock_guard guard(lock);
    const auto found = endpoints.find(local_id);
    if (found == endpoints.end() || found->second.kind != Kind::Local) return SCE_VOICE_QOS_ERROR_INVALID_ID;
    const auto* bytes = static_cast<const std::uint8_t*>(value);
    found->second.attributes[attribute_id].assign(bytes, bytes + size);
    return 0;
}

// Unset attributes report full volume, a silent microphone and zero for everything else.
int APS5_VABI sceVoiceQoSGetLocalEndpointAttribute_nid_postfix(int local_id, int attribute_id, void* value, int size) {
    if (!value || size <= 0) return SCE_VOICE_QOS_ERROR_INVALID_ARGUMENT;
    std::lock_guard guard(lock);
    const auto found = endpoints.find(local_id);
    if (found == endpoints.end() || found->second.kind != Kind::Local) return SCE_VOICE_QOS_ERROR_INVALID_ID;
    std::memset(value, 0, static_cast<std::size_t>(size));
    const auto stored = found->second.attributes.find(attribute_id);
    if (stored != found->second.attributes.end()) {
        std::memcpy(value, stored->second.data(), std::min<std::size_t>(stored->second.size(), static_cast<std::size_t>(size)));
    } else if ((attribute_id == ATTRIBUTE_MIC_VOLUME || attribute_id == ATTRIBUTE_SPEAKER_VOLUME) && size >= static_cast<int>(sizeof(float))) {
        const float fullVolume = 1.0f;
        std::memcpy(value, &fullVolume, sizeof(fullVolume));
    } else if (attribute_id == ATTRIBUTE_SILENT_STATE && size >= static_cast<int>(sizeof(int))) {
        const int silent = 1;
        std::memcpy(value, &silent, sizeof(silent));
    }
    return 0;
}

int APS5_VABI sceVoiceQoSWritePacket_nid_postfix(int connection_id, const void* data, uint32_t* size) {
    if (!data || !size) return SCE_VOICE_QOS_ERROR_INVALID_ARGUMENT;
    return exists(connection_id, Kind::Connection) ? 0 : SCE_VOICE_QOS_ERROR_INVALID_ID;
}

int APS5_VABI sceVoiceQoSReadPacket_nid_postfix(int connection_id, void* data, uint32_t* size) {
    if (!data || !size) return SCE_VOICE_QOS_ERROR_INVALID_ARGUMENT;
    if (!exists(connection_id, Kind::Connection)) return SCE_VOICE_QOS_ERROR_INVALID_ID;
    *size = 0;
    return 0;
}

}
