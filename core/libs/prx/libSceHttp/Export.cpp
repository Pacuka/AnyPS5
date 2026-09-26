#include <cerrno>
#include <cstdint>
#include <cstddef>
#include <cstring>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

// The console is offline: HTTP objects are tracked so the API behaves consistently, but every
// request fails at send time with a DNS error, the way it does on a PS5 without a network.

namespace {

constexpr int SCE_HTTP_ERROR_BEFORE_INIT = static_cast<int>(0x80431001);
constexpr int SCE_HTTP_ERROR_INVALID_ID = static_cast<int>(0x80431100);
constexpr int SCE_HTTP_ERROR_OUT_OF_SIZE = static_cast<int>(0x80431104);
constexpr int SCE_HTTP_ERROR_INVALID_VALUE = static_cast<int>(0x804311FE);
constexpr int SCE_HTTP_ERROR_INVALID_URL = static_cast<int>(0x80433060);
constexpr int SCE_HTTP_ERROR_BEFORE_SEND = static_cast<int>(0x80431065);
constexpr int SCE_HTTP_ERROR_RESOLVER_ENODNS = static_cast<int>(0x80436002);
constexpr int GUEST_ENETDOWN = 50;

constexpr std::uint32_t URI_BUILD_WITH_SCHEME = 0x01;
constexpr std::uint32_t URI_BUILD_WITH_HOSTNAME = 0x02;
constexpr std::uint32_t URI_BUILD_WITH_PORT = 0x04;
constexpr std::uint32_t URI_BUILD_WITH_PATH = 0x08;
constexpr std::uint32_t URI_BUILD_WITH_USERNAME = 0x10;
constexpr std::uint32_t URI_BUILD_WITH_PASSWORD = 0x20;
constexpr std::uint32_t URI_BUILD_WITH_QUERY = 0x40;
constexpr std::uint32_t URI_BUILD_WITH_FRAGMENT = 0x80;

enum class Kind { Context, Template, Connection, Request };

struct Object {
    Kind kind;
    int parent;
    int lastErrno = 0;
};

std::mutex lock;
std::map<int, Object> objects;
std::map<HttpEpoll*, std::unique_ptr<HttpEpoll>> epolls;
int nextId = 1;

int create(Kind kind, int parent, Kind parentKind) {
    std::lock_guard guard(lock);
    if (kind != Kind::Context) {
        const auto found = objects.find(parent);
        if (found == objects.end() || found->second.kind != parentKind) return SCE_HTTP_ERROR_INVALID_ID;
    }
    const int id = nextId++;
    objects.emplace(id, Object{kind, parent});
    return id;
}

int destroy(int id, Kind kind) {
    std::lock_guard guard(lock);
    const auto found = objects.find(id);
    if (found == objects.end() || found->second.kind != kind) return SCE_HTTP_ERROR_INVALID_ID;
    objects.erase(found);
    return 0;
}

// Setters accept a template, connection or request id.
int configure(int id) {
    std::lock_guard guard(lock);
    const auto found = objects.find(id);
    return found == objects.end() || found->second.kind == Kind::Context ? SCE_HTTP_ERROR_INVALID_ID : 0;
}

int requireRequest(int id) {
    std::lock_guard guard(lock);
    const auto found = objects.find(id);
    return found == objects.end() || found->second.kind != Kind::Request ? SCE_HTTP_ERROR_INVALID_ID : 0;
}

struct ParsedUri {
    bool opaque = false;
    std::string scheme, username, password, hostname, path, query, fragment;
    std::uint16_t port = 0;
};

bool parseUri(const std::string& url, ParsedUri& uri) {
    const auto colon = url.find(':');
    if (colon == std::string::npos || colon == 0) return false;
    uri.scheme = url.substr(0, colon);
    std::string rest = url.substr(colon + 1);
    const auto fragmentStart = rest.find('#');
    if (fragmentStart != std::string::npos) {
        uri.fragment = rest.substr(fragmentStart);
        rest.erase(fragmentStart);
    }
    const auto queryStart = rest.find('?');
    if (queryStart != std::string::npos) {
        uri.query = rest.substr(queryStart);
        rest.erase(queryStart);
    }
    if (rest.compare(0, 2, "//") != 0) {
        uri.opaque = true;
        uri.path = rest;
        return true;
    }
    rest.erase(0, 2);
    const auto pathStart = rest.find('/');
    std::string authority = rest.substr(0, pathStart);
    uri.path = pathStart == std::string::npos ? "/" : rest.substr(pathStart);
    const auto at = authority.rfind('@');
    if (at != std::string::npos) {
        const std::string credentials = authority.substr(0, at);
        const auto separator = credentials.find(':');
        uri.username = credentials.substr(0, separator);
        if (separator != std::string::npos) uri.password = credentials.substr(separator + 1);
        authority.erase(0, at + 1);
    }
    const auto portStart = authority.rfind(':');
    if (portStart != std::string::npos && authority.find(']', portStart) == std::string::npos) {
        const std::string port = authority.substr(portStart + 1);
        if (port.empty() || port.find_first_not_of("0123456789") != std::string::npos || port.size() > 5 || std::stoul(port) > 65535) return false;
        uri.port = static_cast<std::uint16_t>(std::stoul(port));
        authority.erase(portStart);
    } else if (uri.scheme == "http") {
        uri.port = 80;
    } else if (uri.scheme == "https") {
        uri.port = 443;
    }
    uri.hostname = authority;
    return !uri.hostname.empty();
}

int requireValidUrl(const char* url) {
    ParsedUri uri;
    return url && parseUri(url, uri) && !uri.opaque ? 0 : SCE_HTTP_ERROR_INVALID_URL;
}

// Writes text plus terminator to out when it fits in prepare bytes; always reports the size.
int emit(const std::string& text, char* out, size_t* require, size_t prepare) {
    if (require) *require = text.size() + 1;
    if (!out) return 0;
    if (prepare < text.size() + 1) return SCE_HTTP_ERROR_OUT_OF_SIZE;
    std::memcpy(out, text.c_str(), text.size() + 1);
    return 0;
}

}

extern "C" {

int APS5_VABI sceHttpInit_nid_postfix(int memid, int ssl_ctx_id, uint64_t pool_size) {
    (void)memid;
    (void)ssl_ctx_id;
    if (pool_size == 0) return SCE_HTTP_ERROR_INVALID_VALUE;
    return create(Kind::Context, 0, Kind::Context);
}

int APS5_VABI sceHttpTerm_nid_postfix(int http_ctx_id) {
    const int result = destroy(http_ctx_id, Kind::Context);
    return result == SCE_HTTP_ERROR_INVALID_ID ? SCE_HTTP_ERROR_BEFORE_INIT : result;
}

int APS5_VABI sceHttpCreateTemplate(int http_ctx_id, const char* user_agent, int http_ver, int is_auto_proxy_conf) {
    (void)user_agent;
    (void)http_ver;
    (void)is_auto_proxy_conf;
    return create(Kind::Template, http_ctx_id, Kind::Context);
}

int APS5_VABI sceHttpDeleteTemplate(int tmpl_id) {
    return destroy(tmpl_id, Kind::Template);
}

int APS5_VABI sceHttpCreateConnection(int tmpl_id, const char* server_name, const char* scheme, uint16_t port, int enable_keep_alive) {
    (void)port;
    (void)enable_keep_alive;
    if (!server_name || !scheme) return SCE_HTTP_ERROR_INVALID_VALUE;
    return create(Kind::Connection, tmpl_id, Kind::Template);
}

int APS5_VABI sceHttpCreateConnectionWithURL(int tmpl_id, const char* url, int enable_keep_alive) {
    (void)enable_keep_alive;
    if (const int result = requireValidUrl(url); result != 0) return result;
    return create(Kind::Connection, tmpl_id, Kind::Template);
}

int APS5_VABI sceHttpDeleteConnection(int conn_id) {
    return destroy(conn_id, Kind::Connection);
}

int APS5_VABI sceHttpCreateRequest(int conn_id, int method, const char* path, uint64_t content_length) {
    (void)method;
    (void)content_length;
    if (!path) return SCE_HTTP_ERROR_INVALID_VALUE;
    return create(Kind::Request, conn_id, Kind::Connection);
}

int APS5_VABI sceHttpCreateRequestWithURL_nid_postfix(int conn_id, int method, const char* url, uint64_t content_length) {
    (void)method;
    (void)content_length;
    if (const int result = requireValidUrl(url); result != 0) return result;
    return create(Kind::Request, conn_id, Kind::Connection);
}

int APS5_VABI sceHttpCreateRequestWithURL2(int conn_id, const char* method, const char* url, uint64_t content_length) {
    (void)content_length;
    if (!method) return SCE_HTTP_ERROR_INVALID_VALUE;
    if (const int result = requireValidUrl(url); result != 0) return result;
    return create(Kind::Request, conn_id, Kind::Connection);
}

int APS5_VABI sceHttpDeleteRequest(int req_id) {
    return destroy(req_id, Kind::Request);
}

int APS5_VABI sceHttpAbortRequest(int request_id) {
    return requireRequest(request_id);
}

int APS5_VABI sceHttpAddRequestHeader(int id, const char* name, const char* value, uint32_t mode) {
    (void)mode;
    if (!name || !value) return SCE_HTTP_ERROR_INVALID_VALUE;
    return configure(id);
}

int APS5_VABI sceHttpSetRequestContentLength(int request_id, uint64_t content_length) {
    (void)content_length;
    return requireRequest(request_id);
}

int APS5_VABI sceHttpSetAuthEnabled(int id, int enable) {
    (void)enable;
    return configure(id);
}

int APS5_VABI sceHttpSetAutoRedirect(int id, int enable) {
    (void)enable;
    return configure(id);
}

int APS5_VABI sceHttpSetConnectTimeOut(int id, uint32_t usec) {
    (void)usec;
    return configure(id);
}

int APS5_VABI sceHttpSetRecvTimeOut(int id, uint32_t usec) {
    (void)usec;
    return configure(id);
}

int APS5_VABI sceHttpSetSendTimeOut(int id, uint32_t usec) {
    (void)usec;
    return configure(id);
}

int APS5_VABI sceHttpSetResolveTimeOut(int id, uint32_t usec) {
    (void)usec;
    return configure(id);
}

int APS5_VABI sceHttpSetResolveRetry(int id, int32_t retry) {
    (void)retry;
    return configure(id);
}

int APS5_VABI sceHttpSetNonblock(int id, int enable) {
    (void)enable;
    return configure(id);
}

int APS5_VABI sceHttpsDisableOption(int id, uint32_t ssl_flags) {
    (void)ssl_flags;
    return configure(id);
}

int APS5_VABI sceHttpsSetMinSslVersion(int id, uint32_t ssl_version) {
    (void)ssl_version;
    return configure(id);
}

int APS5_VABI sceHttpsSetSslCallback(int id, HttpsCallback cbfunc, void* user_arg) {
    (void)cbfunc;
    (void)user_arg;
    return configure(id);
}

int APS5_VABI sceHttpSendRequest(int request_id, const void* post_data, size_t size) {
    (void)post_data;
    (void)size;
    std::lock_guard guard(lock);
    const auto found = objects.find(request_id);
    if (found == objects.end() || found->second.kind != Kind::Request) return SCE_HTTP_ERROR_INVALID_ID;
    found->second.lastErrno = GUEST_ENETDOWN;
    return SCE_HTTP_ERROR_RESOLVER_ENODNS;
}

int APS5_VABI sceHttpGetLastErrno_nid_postfix(int request_id, int* errNum) {
    if (!errNum) return SCE_HTTP_ERROR_INVALID_VALUE;
    std::lock_guard guard(lock);
    const auto found = objects.find(request_id);
    if (found == objects.end() || found->second.kind != Kind::Request) return SCE_HTTP_ERROR_INVALID_ID;
    *errNum = found->second.lastErrno;
    return 0;
}

int APS5_VABI sceHttpGetStatusCode(int request_id, int* status_code) {
    if (!status_code) return SCE_HTTP_ERROR_INVALID_VALUE;
    if (const int result = requireRequest(request_id); result != 0) return result;
    return SCE_HTTP_ERROR_BEFORE_SEND;
}

int APS5_VABI sceHttpGetResponseContentLength(int request_id, int* result, uint64_t* content_length) {
    if (!result || !content_length) return SCE_HTTP_ERROR_INVALID_VALUE;
    if (const int error = requireRequest(request_id); error != 0) return error;
    return SCE_HTTP_ERROR_BEFORE_SEND;
}

int APS5_VABI sceHttpGetAllResponseHeaders(int request_id, char** header, size_t* header_size) {
    if (!header || !header_size) return SCE_HTTP_ERROR_INVALID_VALUE;
    if (const int result = requireRequest(request_id); result != 0) return result;
    return SCE_HTTP_ERROR_BEFORE_SEND;
}

int APS5_VABI sceHttpReadData_nid_postfix(int request_id, void* data, size_t size) {
    if (!data && size != 0) return SCE_HTTP_ERROR_INVALID_VALUE;
    if (const int result = requireRequest(request_id); result != 0) return result;
    return SCE_HTTP_ERROR_BEFORE_SEND;
}

int APS5_VABI sceHttpCreateEpoll(int http_ctx_id, HttpEpollHandle* eh) {
    if (!eh) return SCE_HTTP_ERROR_INVALID_VALUE;
    std::lock_guard guard(lock);
    const auto found = objects.find(http_ctx_id);
    if (found == objects.end() || found->second.kind != Kind::Context) return SCE_HTTP_ERROR_INVALID_ID;
    auto handle = std::make_unique<HttpEpoll>();
    *eh = handle.get();
    epolls.emplace(handle.get(), std::move(handle));
    return 0;
}

int APS5_VABI sceHttpDestroyEpoll(int http_ctx_id, HttpEpollHandle eh) {
    (void)http_ctx_id;
    std::lock_guard guard(lock);
    return epolls.erase(eh) == 1 ? 0 : SCE_HTTP_ERROR_INVALID_VALUE;
}

int APS5_VABI sceHttpSetEpoll(int id, HttpEpollHandle eh, void* user_arg) {
    (void)user_arg;
    {
        std::lock_guard guard(lock);
        if (!epolls.count(eh)) return SCE_HTTP_ERROR_INVALID_VALUE;
    }
    return requireRequest(id);
}

int APS5_VABI sceHttpUnsetEpoll(int id) {
    return requireRequest(id);
}

// Requests fail when they are sent, so no request is ever pending on an epoll handle.
int APS5_VABI sceHttpWaitRequest(HttpEpollHandle eh, HttpNBEvent* nbev, int maxevents, int timeout) {
    (void)timeout;
    if (!nbev || maxevents <= 0) return SCE_HTTP_ERROR_INVALID_VALUE;
    std::lock_guard guard(lock);
    return epolls.count(eh) ? 0 : SCE_HTTP_ERROR_INVALID_VALUE;
}

int APS5_VABI sceHttpUriParse(SceHttpUriElement* out, const char* src_url, void* pool, size_t* require, size_t prepare) {
    if (!src_url) return SCE_HTTP_ERROR_INVALID_VALUE;
    ParsedUri uri;
    if (!parseUri(src_url, uri)) return SCE_HTTP_ERROR_INVALID_URL;
    const std::string* parts[] = {&uri.scheme, &uri.username, &uri.password, &uri.hostname, &uri.path, &uri.query, &uri.fragment};
    std::size_t needed = 0;
    for (const auto* part : parts) needed += part->size() + 1;
    if (require) *require = needed;
    if (!pool) return 0;
    if (!out) return SCE_HTTP_ERROR_INVALID_VALUE;
    if (prepare < needed) return SCE_HTTP_ERROR_OUT_OF_SIZE;
    char* cursor = static_cast<char*>(pool);
    char** targets[] = {&out->scheme, &out->username, &out->password, &out->hostname, &out->path, &out->query, &out->fragment};
    for (std::size_t index = 0; index < 7; ++index) {
        std::memcpy(cursor, parts[index]->c_str(), parts[index]->size() + 1);
        *targets[index] = cursor;
        cursor += parts[index]->size() + 1;
    }
    out->opaque = uri.opaque ? 1 : 0;
    out->port = uri.port;
    return 0;
}

int APS5_VABI sceHttpUriBuild(char* out, size_t* require, size_t prepare, const SceHttpUriElement* src_element, uint32_t option) {
    if (!src_element) return SCE_HTTP_ERROR_INVALID_VALUE;
    const auto& uri = *src_element;
    std::string text;
    if ((option & URI_BUILD_WITH_SCHEME) && uri.scheme) text += std::string(uri.scheme) + (uri.opaque ? ":" : "://");
    if ((option & URI_BUILD_WITH_USERNAME) && uri.username && *uri.username) {
        text += uri.username;
        if ((option & URI_BUILD_WITH_PASSWORD) && uri.password && *uri.password) text += std::string(":") + uri.password;
        text += "@";
    }
    if ((option & URI_BUILD_WITH_HOSTNAME) && uri.hostname) text += uri.hostname;
    if ((option & URI_BUILD_WITH_PORT) && uri.port != 0) text += ":" + std::to_string(uri.port);
    if ((option & URI_BUILD_WITH_PATH) && uri.path) text += uri.path;
    if ((option & URI_BUILD_WITH_QUERY) && uri.query) text += uri.query;
    if ((option & URI_BUILD_WITH_FRAGMENT) && uri.fragment) text += uri.fragment;
    return emit(text, out, require, prepare);
}

// Percent-encodes every byte outside the RFC 3986 unreserved set.
int APS5_VABI sceHttpUriEscape(char* out, size_t* require, size_t prepare, const char* in) {
    if (!in) return SCE_HTTP_ERROR_INVALID_VALUE;
    static constexpr char hex[] = "0123456789ABCDEF";
    std::string text;
    for (const auto* cursor = reinterpret_cast<const unsigned char*>(in); *cursor; ++cursor) {
        const unsigned char c = *cursor;
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-' || c == '.' || c == '_' || c == '~') {
            text.push_back(static_cast<char>(c));
        } else {
            text.push_back('%');
            text.push_back(hex[c >> 4]);
            text.push_back(hex[c & 0xF]);
        }
    }
    return emit(text, out, require, prepare);
}

}
