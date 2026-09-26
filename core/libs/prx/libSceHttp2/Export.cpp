#include <cstdint>
#include <cstddef>
#include <map>
#include <mutex>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

// The console is offline: HTTP/2 templates and requests are tracked, and every request fails at
// send time with a DNS error, like a PS5 without a network connection.

namespace {

constexpr int SCE_HTTP2_ERROR_INVALID_ID = static_cast<int>(0x80431100);
constexpr int SCE_HTTP2_ERROR_INVALID_VALUE = static_cast<int>(0x804311FE);
constexpr int SCE_HTTP2_ERROR_BEFORE_SEND = static_cast<int>(0x80431065);
constexpr int SCE_HTTP2_ERROR_RESOLVER_ENODNS = static_cast<int>(0x80436002);

enum class Kind { Context, Template, Request };

std::mutex lock;
std::map<int, Kind> objects;
int nextId = 1;

int create(Kind kind, int parent, Kind parentKind) {
    std::lock_guard guard(lock);
    if (kind != Kind::Context) {
        const auto found = objects.find(parent);
        if (found == objects.end() || found->second != parentKind) return SCE_HTTP2_ERROR_INVALID_ID;
    }
    const int id = nextId++;
    objects.emplace(id, kind);
    return id;
}

int destroy(int id, Kind kind) {
    std::lock_guard guard(lock);
    const auto found = objects.find(id);
    if (found == objects.end() || found->second != kind) return SCE_HTTP2_ERROR_INVALID_ID;
    objects.erase(found);
    return 0;
}

int configure(int id) {
    std::lock_guard guard(lock);
    const auto found = objects.find(id);
    return found == objects.end() || found->second == Kind::Context ? SCE_HTTP2_ERROR_INVALID_ID : 0;
}

int requireRequest(int id) {
    std::lock_guard guard(lock);
    const auto found = objects.find(id);
    return found == objects.end() || found->second != Kind::Request ? SCE_HTTP2_ERROR_INVALID_ID : 0;
}

}

extern "C" {

int APS5_VABI sceHttp2Init(int libnet_mem_id, int libssl_ctx_id, size_t pool_size, int max_concurrent_request) {
    (void)libnet_mem_id;
    (void)libssl_ctx_id;
    if (pool_size == 0 || max_concurrent_request <= 0) return SCE_HTTP2_ERROR_INVALID_VALUE;
    return create(Kind::Context, 0, Kind::Context);
}

int APS5_VABI sceHttp2Term(int lib_http2_ctx_id) {
    return destroy(lib_http2_ctx_id, Kind::Context);
}

int APS5_VABI sceHttp2CreateTemplate(int lib_http2_ctx_id, const char* user_agent, int http_ver, int is_auto_proxy_conf) {
    (void)user_agent;
    (void)http_ver;
    (void)is_auto_proxy_conf;
    return create(Kind::Template, lib_http2_ctx_id, Kind::Context);
}

int APS5_VABI sceHttp2DeleteTemplate(int tmpl_id) {
    return destroy(tmpl_id, Kind::Template);
}

int APS5_VABI sceHttp2CreateRequestWithURL(int tmpl_id, const char* method, const char* url, uint64_t content_length) {
    (void)content_length;
    if (!method || !url) return SCE_HTTP2_ERROR_INVALID_VALUE;
    return create(Kind::Request, tmpl_id, Kind::Template);
}

int APS5_VABI sceHttp2DeleteRequest(int req_id) {
    return destroy(req_id, Kind::Request);
}

int APS5_VABI sceHttp2AbortRequest_nid_postfix(int req_id) {
    return requireRequest(req_id);
}

int APS5_VABI sceHttp2AddRequestHeader(int id, const char* name, const char* value, uint32_t mode) {
    (void)mode;
    if (!name || !value) return SCE_HTTP2_ERROR_INVALID_VALUE;
    return configure(id);
}

int APS5_VABI sceHttp2SetRequestContentLength(int id, uint64_t content_length) {
    (void)content_length;
    return requireRequest(id);
}

int APS5_VABI sceHttp2SetAuthEnabled(int id, int is_enable) {
    (void)is_enable;
    return configure(id);
}

int APS5_VABI sceHttp2SetAutoRedirect(int id, int enable) {
    (void)enable;
    return configure(id);
}

int APS5_VABI sceHttp2SetConnectionWaitTimeOut(int id, uint32_t usec) {
    (void)usec;
    return configure(id);
}

int APS5_VABI sceHttp2SetConnectTimeOut(int id, uint32_t usec) {
    (void)usec;
    return configure(id);
}

int APS5_VABI sceHttp2SetInflateGZIPEnabled(int id, int enable) {
    (void)enable;
    return configure(id);
}

int APS5_VABI sceHttp2SetRecvTimeOut(int id, uint32_t usec) {
    (void)usec;
    return configure(id);
}

int APS5_VABI sceHttp2SetSendTimeOut(int id, uint32_t usec) {
    (void)usec;
    return configure(id);
}

int APS5_VABI sceHttp2SetResolveTimeOut(int id, uint32_t usec) {
    (void)usec;
    return configure(id);
}

int APS5_VABI sceHttp2SetTimeOut(int id, uint32_t usec) {
    (void)usec;
    return configure(id);
}

int APS5_VABI sceHttp2SetRedirectCallback(int id, void* cb_func, void* user_arg) {
    (void)cb_func;
    (void)user_arg;
    return configure(id);
}

int APS5_VABI sceHttp2SetSslCallback(int id, void* cb_func, void* user_arg) {
    (void)cb_func;
    (void)user_arg;
    return configure(id);
}

int APS5_VABI sceHttp2SslDisableOption(int id, uint32_t ssl_flags) {
    (void)ssl_flags;
    return configure(id);
}

int APS5_VABI sceHttp2SslEnableOption(int id, uint32_t ssl_flags) {
    (void)ssl_flags;
    return configure(id);
}

int APS5_VABI sceHttp2SetMinSslVersion_nid_postfix(int id, uint32_t ssl_version) {
    (void)ssl_version;
    return configure(id);
}

int APS5_VABI sceHttp2SendRequest(int req_id, const void* post_data, size_t size) {
    (void)post_data;
    (void)size;
    if (const int result = requireRequest(req_id); result != 0) return result;
    return SCE_HTTP2_ERROR_RESOLVER_ENODNS;
}

int APS5_VABI sceHttp2SendRequestAsync(int req_id, const void* post_data, size_t size, void* kqueue_option, void* option) {
    (void)kqueue_option;
    (void)option;
    return sceHttp2SendRequest(req_id, post_data, size);
}

int APS5_VABI sceHttp2GetStatusCode(int req_id, int* status_code) {
    if (!status_code) return SCE_HTTP2_ERROR_INVALID_VALUE;
    if (const int result = requireRequest(req_id); result != 0) return result;
    return SCE_HTTP2_ERROR_BEFORE_SEND;
}

int APS5_VABI sceHttp2GetResponseContentLength(int req_id, int* result, uint64_t* content_length) {
    if (!result || !content_length) return SCE_HTTP2_ERROR_INVALID_VALUE;
    if (const int error = requireRequest(req_id); error != 0) return error;
    return SCE_HTTP2_ERROR_BEFORE_SEND;
}

int APS5_VABI sceHttp2GetAllResponseHeaders(int req_id, char** header, size_t* header_size) {
    if (!header || !header_size) return SCE_HTTP2_ERROR_INVALID_VALUE;
    if (const int result = requireRequest(req_id); result != 0) return result;
    return SCE_HTTP2_ERROR_BEFORE_SEND;
}

int APS5_VABI sceHttp2ReadData(int req_id, void* data, size_t size) {
    if (!data && size != 0) return SCE_HTTP2_ERROR_INVALID_VALUE;
    if (const int result = requireRequest(req_id); result != 0) return result;
    return SCE_HTTP2_ERROR_BEFORE_SEND;
}

int APS5_VABI sceHttp2ReadDataAsync(int req_id, void* data, size_t size, void* kqueue_option, void* option) {
    (void)kqueue_option;
    (void)option;
    return sceHttp2ReadData(req_id, data, size);
}

// Asynchronous sends fail immediately, so no request ever has a pending completion.
int APS5_VABI sceHttp2WaitAsync(int req_id, Http2AsyncResult* result, uint32_t* timeout, void* option) {
    (void)timeout;
    (void)option;
    if (!result) return SCE_HTTP2_ERROR_INVALID_VALUE;
    if (const int error = requireRequest(req_id); error != 0) return error;
    return SCE_HTTP2_ERROR_BEFORE_SEND;
}

}
