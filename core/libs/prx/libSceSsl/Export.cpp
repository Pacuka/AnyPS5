#include <atomic>
#include <cstdint>
#include <cstddef>
#include <cstring>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

// The console is offline, so SSL contexts exist but carry no system CA certificates.

namespace {

constexpr int SCE_SSL_ERROR_INVALID_VALUE = static_cast<int>(0x8095F007);

struct SslCaCerts {
    void* certData;
    std::uint64_t certDataNum;
    void* pool;
};

std::atomic<int> nextContextId{1};

}

extern "C" {

int APS5_VABI sceSslInit_nid_postfix(uint64_t pool_size) {
    if (pool_size == 0) return SCE_SSL_ERROR_INVALID_VALUE;
    return nextContextId++;
}

int APS5_VABI sceSslTerm_nid_postfix(int ssl_ctx_id) {
    return ssl_ctx_id > 0 ? 0 : SCE_SSL_ERROR_INVALID_VALUE;
}

int APS5_VABI sceSslGetCaCerts(int ssl_ctx_id, void* ca_certs) {
    if (ssl_ctx_id <= 0 || !ca_certs) return SCE_SSL_ERROR_INVALID_VALUE;
    const SslCaCerts empty{};
    std::memcpy(ca_certs, &empty, sizeof(empty));
    return 0;
}

int APS5_VABI sceSslFreeCaCerts(int ssl_ctx_id, void* ca_certs) {
    if (ssl_ctx_id <= 0 || !ca_certs) return SCE_SSL_ERROR_INVALID_VALUE;
    return 0;
}

}
