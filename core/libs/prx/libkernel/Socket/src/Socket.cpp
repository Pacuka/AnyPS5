#include <algorithm>
#include <cerrno>
#include <cstdint>
#include <cstddef>
#include <cstring>
#include <stdexcept>
#include <string>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/GuestErrno.hpp"
#include "prx/libkernel/Socket/include/NetBridge.hpp"

#ifndef _WIN32
#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <signal.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

namespace {

constexpr int GUEST_AF_INET = 2;
constexpr int GUEST_AF_INET6 = 28;
constexpr int GUEST_SOL_SOCKET = 0xFFFF;
constexpr int GUEST_IPPROTO_IP = 0;
constexpr int GUEST_IPPROTO_TCP = 6;
constexpr int GUEST_SOCK_STREAM = 1;
constexpr int GUEST_SOCK_DGRAM = 2;
constexpr int GUEST_SOCK_RAW = 3;
constexpr int GUEST_SOCK_DGRAM_P2P = 6;
constexpr int GUEST_SOCK_STREAM_P2P = 10;
constexpr int GUEST_SOCK_CLOEXEC = 0x10000000;
constexpr int GUEST_SOCK_NONBLOCK = 0x20000000;
constexpr int GUEST_O_NONBLOCK = 0x4;
constexpr int GUEST_SO_ERROR = 0x1007;
constexpr int GUEST_SO_SNDTIMEO = 0x1005;
constexpr int GUEST_SO_RCVTIMEO = 0x1006;
constexpr int GUEST_SO_NBIO = 0x1200;
constexpr std::size_t MAX_SOCKADDR = 128;

#ifdef _WIN32
[[noreturn]] void unsupported(const char* function) {
    throw std::runtime_error(std::string(function) + " is not implemented on Windows yet");
}
#else

int failure() {
    return -GuestErrno(errno);
}

// FreeBSD sockaddr: {uint8 len, uint8 family, data...}; Linux: {uint16 family, data...}.
int toHostAddress(const void* guest, std::uint32_t length, sockaddr_storage& host, socklen_t& hostLength) {
    if (!guest || length < 2 || length > sizeof(host)) return -GuestErrno(EINVAL);
    const auto* bytes = static_cast<const std::uint8_t*>(guest);
    const int family = NetGuestFamilyToHost_nid_no_patch(bytes[1]);
    if (family < 0) return -GuestErrno(EAFNOSUPPORT);
    std::memset(&host, 0, sizeof(host));
    std::memcpy(&host, bytes, length);
    host.ss_family = static_cast<sa_family_t>(family);
    hostLength = family == AF_INET ? sizeof(sockaddr_in) : family == AF_INET6 ? sizeof(sockaddr_in6) : length;
    return 0;
}

void toGuestAddress(const sockaddr_storage& host, socklen_t hostLength, void* guest, std::uint32_t* length) {
    if (!guest || !length) return;
    std::uint8_t converted[MAX_SOCKADDR]{};
    const std::size_t size = std::min<std::size_t>(hostLength, sizeof(converted));
    std::memcpy(converted, &host, size);
    converted[0] = static_cast<std::uint8_t>(size);
    converted[1] = static_cast<std::uint8_t>(NetHostFamilyToGuest_nid_no_patch(host.ss_family));
    std::memcpy(guest, converted, std::min<std::size_t>(size, *length));
    *length = static_cast<std::uint32_t>(size);
}

int toHostMessageFlags(int flags) {
    int host = MSG_NOSIGNAL;
    if (flags & 0x1) host |= MSG_OOB;
    if (flags & 0x2) host |= MSG_PEEK;
    if (flags & 0x4) host |= MSG_DONTROUTE;
    if (flags & 0x8) host |= MSG_EOR;
    if (flags & 0x40) host |= MSG_WAITALL;
    if (flags & 0x80) host |= MSG_DONTWAIT;
    // 0x100000/0x200000 request PSN crypto/signatures on P2P sockets, which plain host sockets lack.
    return host;
}

bool toHostOption(int level, int name, int& hostLevel, int& hostName) {
    if (level == GUEST_SOL_SOCKET) {
        hostLevel = SOL_SOCKET;
        switch (name) {
        case 0x0004: hostName = SO_REUSEADDR; return true;
        case 0x0008: hostName = SO_KEEPALIVE; return true;
        case 0x0010: hostName = SO_DONTROUTE; return true;
        case 0x0020: hostName = SO_BROADCAST; return true;
        case 0x0080: hostName = SO_LINGER; return true;
        case 0x0100: hostName = SO_OOBINLINE; return true;
        case 0x0200: hostName = SO_REUSEPORT; return true;
        case 0x1001: hostName = SO_SNDBUF; return true;
        case 0x1002: hostName = SO_RCVBUF; return true;
        case 0x1003: hostName = SO_SNDLOWAT; return true;
        case 0x1004: hostName = SO_RCVLOWAT; return true;
        case GUEST_SO_SNDTIMEO: hostName = SO_SNDTIMEO; return true;
        case GUEST_SO_RCVTIMEO: hostName = SO_RCVTIMEO; return true;
        case GUEST_SO_ERROR: hostName = SO_ERROR; return true;
        case 0x1008: hostName = SO_TYPE; return true;
        default: return false;
        }
    }
    if (level == GUEST_IPPROTO_IP) {
        hostLevel = IPPROTO_IP;
        switch (name) {
        case 1: hostName = IP_OPTIONS; return true;
        case 2: hostName = IP_HDRINCL; return true;
        case 3: hostName = IP_TOS; return true;
        case 4: hostName = IP_TTL; return true;
        case 9: hostName = IP_MULTICAST_IF; return true;
        case 10: hostName = IP_MULTICAST_TTL; return true;
        case 11: hostName = IP_MULTICAST_LOOP; return true;
        case 12: hostName = IP_ADD_MEMBERSHIP; return true;
        case 13: hostName = IP_DROP_MEMBERSHIP; return true;
        default: return false;
        }
    }
    if (level == GUEST_IPPROTO_TCP) {
        hostLevel = IPPROTO_TCP;
        switch (name) {
        case 1: hostName = TCP_NODELAY; return true;
        case 2: hostName = TCP_MAXSEG; return true;
        default: return false;
        }
    }
    return false;
}

int setNonblocking(int s, bool enabled) {
    const int flags = ::fcntl(s, F_GETFL);
    if (flags < 0) return failure();
    return ::fcntl(s, F_SETFL, enabled ? flags | O_NONBLOCK : flags & ~O_NONBLOCK) == 0 ? 0 : failure();
}

#endif

int posixResult(std::int64_t result) {
    if (result >= 0) return static_cast<int>(result);
    errno = static_cast<int>(-result);
    return -1;
}

}

extern "C" {

int NetGuestFamilyToHost_nid_no_patch(int family) {
#ifdef _WIN32
    (void)family;
    unsupported(__func__);
#else
    switch (family) {
    case 0: return AF_UNSPEC;
    case GUEST_AF_INET: return AF_INET;
    case GUEST_AF_INET6: return AF_INET6;
    default: return -1;
    }
#endif
}

int NetHostFamilyToGuest_nid_no_patch(int family) {
#ifdef _WIN32
    (void)family;
    unsupported(__func__);
#else
    return family == AF_INET ? GUEST_AF_INET : family == AF_INET6 ? GUEST_AF_INET6 : 0;
#endif
}

#ifdef _WIN32

int NetSocket_nid_no_patch(int, int, int) { unsupported(__func__); }
int NetClose_nid_no_patch(int) { unsupported(__func__); }
int NetBind_nid_no_patch(int, const void*, std::uint32_t) { unsupported(__func__); }
int NetConnect_nid_no_patch(int, const void*, std::uint32_t) { unsupported(__func__); }
int NetListen_nid_no_patch(int, int) { unsupported(__func__); }
int NetAccept_nid_no_patch(int, void*, std::uint32_t*) { unsupported(__func__); }
int NetShutdown_nid_no_patch(int, int) { unsupported(__func__); }
int NetGetsockname_nid_no_patch(int, void*, std::uint32_t*) { unsupported(__func__); }
int NetGetpeername_nid_no_patch(int, void*, std::uint32_t*) { unsupported(__func__); }
int NetSetsockopt_nid_no_patch(int, int, int, const void*, std::uint32_t, bool) { unsupported(__func__); }
int NetGetsockopt_nid_no_patch(int, int, int, void*, std::uint32_t*, bool) { unsupported(__func__); }
std::int64_t NetSend_nid_no_patch(int, const void*, std::size_t, int, const void*, std::uint32_t) { unsupported(__func__); }
std::int64_t NetRecv_nid_no_patch(int, void*, std::size_t, int, void*, std::uint32_t*) { unsupported(__func__); }
int NetInetPton_nid_no_patch(int, const char*, void*) { unsupported(__func__); }
const char* NetInetNtop_nid_no_patch(int, const void*, char*, std::uint32_t, int*) { unsupported(__func__); }

#else

int NetSocket_nid_no_patch(int family, int type, int protocol) {
    const int hostFamily = NetGuestFamilyToHost_nid_no_patch(family);
    if (hostFamily < 0) return -GuestErrno(EAFNOSUPPORT);
    int hostType = 0;
    switch (type & 0xFFFF) {
    case GUEST_SOCK_STREAM:
    case GUEST_SOCK_STREAM_P2P: hostType = SOCK_STREAM; break;
    case GUEST_SOCK_DGRAM:
    case GUEST_SOCK_DGRAM_P2P: hostType = SOCK_DGRAM; break;
    case GUEST_SOCK_RAW: hostType = SOCK_RAW; break;
    default: return -GuestErrno(EPROTONOSUPPORT);
    }
    if (type & GUEST_SOCK_CLOEXEC) hostType |= SOCK_CLOEXEC;
    if (type & GUEST_SOCK_NONBLOCK) hostType |= SOCK_NONBLOCK;
    const int s = ::socket(hostFamily, hostType, protocol);
    return s < 0 ? failure() : s;
}

int NetClose_nid_no_patch(int s) {
    return ::close(s) == 0 ? 0 : failure();
}

int NetBind_nid_no_patch(int s, const void* addr, std::uint32_t addrlen) {
    sockaddr_storage host;
    socklen_t hostLength;
    if (const int result = toHostAddress(addr, addrlen, host, hostLength); result < 0) return result;
    return ::bind(s, reinterpret_cast<sockaddr*>(&host), hostLength) == 0 ? 0 : failure();
}

int NetConnect_nid_no_patch(int s, const void* addr, std::uint32_t addrlen) {
    sockaddr_storage host;
    socklen_t hostLength;
    if (const int result = toHostAddress(addr, addrlen, host, hostLength); result < 0) return result;
    return ::connect(s, reinterpret_cast<sockaddr*>(&host), hostLength) == 0 ? 0 : failure();
}

int NetListen_nid_no_patch(int s, int backlog) {
    return ::listen(s, backlog) == 0 ? 0 : failure();
}

int NetAccept_nid_no_patch(int s, void* addr, std::uint32_t* addrlen) {
    sockaddr_storage host{};
    socklen_t hostLength = sizeof(host);
    const int accepted = ::accept(s, reinterpret_cast<sockaddr*>(&host), &hostLength);
    if (accepted < 0) return failure();
    toGuestAddress(host, hostLength, addr, addrlen);
    return accepted;
}

int NetShutdown_nid_no_patch(int s, int how) {
    return ::shutdown(s, how) == 0 ? 0 : failure();
}

int NetGetsockname_nid_no_patch(int s, void* addr, std::uint32_t* addrlen) {
    if (!addr || !addrlen) return -GuestErrno(EFAULT);
    sockaddr_storage host{};
    socklen_t hostLength = sizeof(host);
    if (::getsockname(s, reinterpret_cast<sockaddr*>(&host), &hostLength) != 0) return failure();
    toGuestAddress(host, hostLength, addr, addrlen);
    return 0;
}

int NetGetpeername_nid_no_patch(int s, void* addr, std::uint32_t* addrlen) {
    if (!addr || !addrlen) return -GuestErrno(EFAULT);
    sockaddr_storage host{};
    socklen_t hostLength = sizeof(host);
    if (::getpeername(s, reinterpret_cast<sockaddr*>(&host), &hostLength) != 0) return failure();
    toGuestAddress(host, hostLength, addr, addrlen);
    return 0;
}

int NetSetsockopt_nid_no_patch(int s, int level, int name, const void* value, std::uint32_t length, bool sceTimeouts) {
    if (level == GUEST_SOL_SOCKET && name == GUEST_SO_NBIO) {
        if (!value || length < sizeof(int)) return -GuestErrno(EINVAL);
        int enabled;
        std::memcpy(&enabled, value, sizeof(enabled));
        return setNonblocking(s, enabled != 0);
    }
    // PSN-specific socket options (crypto, signatures, virtual ports) have no host equivalent.
    if (level == GUEST_SOL_SOCKET && name >= 0x1100 && name < 0x1200) return 0;
    int hostLevel = 0;
    int hostName = 0;
    if (!toHostOption(level, name, hostLevel, hostName)) return -GuestErrno(ENOPROTOOPT);
    if (sceTimeouts && hostLevel == SOL_SOCKET && (hostName == SO_SNDTIMEO || hostName == SO_RCVTIMEO)) {
        if (!value || length < sizeof(int)) return -GuestErrno(EINVAL);
        int microseconds;
        std::memcpy(&microseconds, value, sizeof(microseconds));
        const timeval timeout{microseconds / 1000000, microseconds % 1000000};
        return ::setsockopt(s, hostLevel, hostName, &timeout, sizeof(timeout)) == 0 ? 0 : failure();
    }
    return ::setsockopt(s, hostLevel, hostName, value, length) == 0 ? 0 : failure();
}

int NetGetsockopt_nid_no_patch(int s, int level, int name, void* value, std::uint32_t* length, bool sceTimeouts) {
    if (!value || !length) return -GuestErrno(EFAULT);
    if (level == GUEST_SOL_SOCKET && name == GUEST_SO_NBIO) {
        if (*length < sizeof(int)) return -GuestErrno(EINVAL);
        const int flags = ::fcntl(s, F_GETFL);
        if (flags < 0) return failure();
        const int enabled = (flags & O_NONBLOCK) != 0;
        std::memcpy(value, &enabled, sizeof(enabled));
        *length = sizeof(enabled);
        return 0;
    }
    int hostLevel = 0;
    int hostName = 0;
    if (!toHostOption(level, name, hostLevel, hostName)) return -GuestErrno(ENOPROTOOPT);
    if (sceTimeouts && hostLevel == SOL_SOCKET && (hostName == SO_SNDTIMEO || hostName == SO_RCVTIMEO)) {
        if (*length < sizeof(int)) return -GuestErrno(EINVAL);
        timeval timeout{};
        socklen_t size = sizeof(timeout);
        if (::getsockopt(s, hostLevel, hostName, &timeout, &size) != 0) return failure();
        const int microseconds = static_cast<int>(timeout.tv_sec * 1000000 + timeout.tv_usec);
        std::memcpy(value, &microseconds, sizeof(microseconds));
        *length = sizeof(microseconds);
        return 0;
    }
    socklen_t size = *length;
    if (::getsockopt(s, hostLevel, hostName, value, &size) != 0) return failure();
    if (hostLevel == SOL_SOCKET && hostName == SO_ERROR && size >= sizeof(int)) {
        int error;
        std::memcpy(&error, value, sizeof(error));
        error = GuestErrno(error);
        std::memcpy(value, &error, sizeof(error));
    }
    *length = size;
    return 0;
}

std::int64_t NetSend_nid_no_patch(int s, const void* buf, std::size_t len, int flags, const void* to, std::uint32_t tolen) {
    if (!buf && len != 0) return -GuestErrno(EFAULT);
    ssize_t sent;
    if (to) {
        sockaddr_storage host;
        socklen_t hostLength;
        if (const int result = toHostAddress(to, tolen, host, hostLength); result < 0) return result;
        sent = ::sendto(s, buf, len, toHostMessageFlags(flags), reinterpret_cast<sockaddr*>(&host), hostLength);
    } else {
        sent = ::send(s, buf, len, toHostMessageFlags(flags));
    }
    return sent < 0 ? failure() : sent;
}

std::int64_t NetRecv_nid_no_patch(int s, void* buf, std::size_t len, int flags, void* from, std::uint32_t* fromlen) {
    if (!buf && len != 0) return -GuestErrno(EFAULT);
    sockaddr_storage host{};
    socklen_t hostLength = sizeof(host);
    const ssize_t received = ::recvfrom(s, buf, len, toHostMessageFlags(flags), from ? reinterpret_cast<sockaddr*>(&host) : nullptr, from ? &hostLength : nullptr);
    if (received < 0) return failure();
    if (from) toGuestAddress(host, hostLength, from, fromlen);
    return received;
}

int NetInetPton_nid_no_patch(int af, const char* src, void* dst) {
    const int family = NetGuestFamilyToHost_nid_no_patch(af);
    if (family != AF_INET && family != AF_INET6) return -GuestErrno(EAFNOSUPPORT);
    if (!src || !dst) return -GuestErrno(EFAULT);
    return ::inet_pton(family, src, dst);
}

const char* NetInetNtop_nid_no_patch(int af, const void* src, char* dst, std::uint32_t size, int* error) {
    const int family = NetGuestFamilyToHost_nid_no_patch(af);
    if (family != AF_INET && family != AF_INET6) {
        *error = GuestErrno(EAFNOSUPPORT);
        return nullptr;
    }
    const char* result = ::inet_ntop(family, src, dst, size);
    if (!result) *error = GuestErrno(errno);
    return result;
}

#endif

int APS5_VABI accept_nid_postfix(int s, void* addr, uint32_t* addrlen) {
    return posixResult(NetAccept_nid_no_patch(s, addr, addrlen));
}

int APS5_VABI bind_nid_postfix(int s, const void* addr, uint32_t addrlen) {
    return posixResult(NetBind_nid_no_patch(s, addr, addrlen));
}

int APS5_VABI connect_nid_postfix(int s, const void* addr, uint32_t addrlen) {
    return posixResult(NetConnect_nid_no_patch(s, addr, addrlen));
}

int APS5_VABI listen_nid_postfix(int s, int backlog) {
    return posixResult(NetListen_nid_no_patch(s, backlog));
}

int APS5_VABI socket_nid_postfix(int family, int type, int protocol) {
    return posixResult(NetSocket_nid_no_patch(family, type, protocol));
}

int APS5_VABI getsockname_nid_postfix(int s, void* addr, uint32_t* addrlen) {
    return posixResult(NetGetsockname_nid_no_patch(s, addr, addrlen));
}

int APS5_VABI getpeername_nid_postfix(int s, void* addr, uint32_t* addrlen) {
    return posixResult(NetGetpeername_nid_no_patch(s, addr, addrlen));
}

int APS5_VABI getsockopt_nid_postfix(int s, int level, int optname, void* optval, uint32_t* optlen) {
    return posixResult(NetGetsockopt_nid_no_patch(s, level, optname, optval, optlen, false));
}

int APS5_VABI setsockopt_nid_postfix(int s, int level, int optname, const void* optval, uint32_t optlen) {
    return posixResult(NetSetsockopt_nid_no_patch(s, level, optname, optval, optlen, false));
}

int APS5_VABI shutdown_nid_postfix(int s, int how) {
    return posixResult(NetShutdown_nid_no_patch(s, how));
}

int64_t APS5_VABI send_nid_postfix(int s, const void* buf, uint64_t len, int flags) {
    const std::int64_t result = NetSend_nid_no_patch(s, buf, len, flags, nullptr, 0);
    return result < 0 ? posixResult(result) : result;
}

int64_t APS5_VABI sendto_nid_postfix(int s, const void* buf, uint64_t len, int flags, const void* addr, uint32_t addrlen) {
    const std::int64_t result = NetSend_nid_no_patch(s, buf, len, flags, addr, addrlen);
    return result < 0 ? posixResult(result) : result;
}

int64_t APS5_VABI recv_nid_postfix(int s, void* buf, uint64_t len, int flags) {
    const std::int64_t result = NetRecv_nid_no_patch(s, buf, len, flags, nullptr, nullptr);
    return result < 0 ? posixResult(result) : result;
}

int64_t APS5_VABI recvfrom_nid_postfix(int s, void* buf, uint64_t len, int flags, void* addr, uint32_t* addrlen) {
    const std::int64_t result = NetRecv_nid_no_patch(s, buf, len, flags, addr, addrlen);
    return result < 0 ? posixResult(result) : result;
}

const char* APS5_VABI inet_ntop_nid_postfix(int af, const void* src, char* dst, uint32_t size) {
    int error = 0;
    const char* result = NetInetNtop_nid_no_patch(af, src, dst, size, &error);
    if (!result) errno = error;
    return result;
}

int APS5_VABI inet_pton_nid_postfix(int af, const char* src, void* dst) {
    return posixResult(NetInetPton_nid_no_patch(af, src, dst));
}

// FreeBSD fd_set and struct timeval share the x86-64 glibc layouts.
int APS5_VABI select_nid_postfix(int nfds, void* readfds, void* writefds, void* exceptfds, const void* timeout) {
#ifdef _WIN32
    (void)nfds; (void)readfds; (void)writefds; (void)exceptfds; (void)timeout;
    unsupported(__func__);
#else
    timeval hostTimeout{};
    if (timeout) std::memcpy(&hostTimeout, timeout, sizeof(hostTimeout));
    const int result = ::select(nfds, static_cast<fd_set*>(readfds), static_cast<fd_set*>(writefds), static_cast<fd_set*>(exceptfds), timeout ? &hostTimeout : nullptr);
    return result < 0 ? GuestPosixFailure(errno) : result;
#endif
}

int APS5_VABI fcntl_nid_postfix(int fd, int command, std::int64_t argument) {
#ifdef _WIN32
    (void)fd; (void)command; (void)argument;
    unsupported(__func__);
#else
    constexpr int F_GETFD_GUEST = 1, F_SETFD_GUEST = 2, F_GETFL_GUEST = 3, F_SETFL_GUEST = 4;
    switch (command) {
    case F_GETFD_GUEST:
    case F_SETFD_GUEST: {
        const int result = ::fcntl(fd, command == F_GETFD_GUEST ? F_GETFD : F_SETFD, static_cast<int>(argument));
        return result < 0 ? GuestPosixFailure(errno) : result;
    }
    case F_GETFL_GUEST: {
        const int flags = ::fcntl(fd, F_GETFL);
        if (flags < 0) return GuestPosixFailure(errno);
        // Only the access-mode bits share values; O_NONBLOCK and O_APPEND (0x8) are translated.
        return (flags & O_ACCMODE) | ((flags & O_NONBLOCK) ? GUEST_O_NONBLOCK : 0) | ((flags & O_APPEND) ? 0x8 : 0);
    }
    case F_SETFL_GUEST: {
        const int flags = ::fcntl(fd, F_GETFL);
        if (flags < 0) return GuestPosixFailure(errno);
        int updated = flags & ~(O_NONBLOCK | O_APPEND);
        if (argument & GUEST_O_NONBLOCK) updated |= O_NONBLOCK;
        if (argument & 0x8) updated |= O_APPEND;
        return ::fcntl(fd, F_SETFL, updated) == 0 ? 0 : GuestPosixFailure(errno);
    }
    default:
        return GuestPosixFailure(EINVAL);
    }
#endif
}

// Guest code never receives host signals, so its signal mask is kept only for reporting.
int APS5_VABI sigprocmask_nid_postfix(int how, const void* set, void* oset) {
    static thread_local std::uint32_t mask[4]{};
    if (oset) std::memcpy(oset, mask, sizeof(mask));
    if (!set) return 0;
    std::uint32_t requested[4];
    std::memcpy(requested, set, sizeof(requested));
    for (int index = 0; index < 4; ++index) {
        switch (how) {
        case 1: mask[index] |= requested[index]; break;
        case 2: mask[index] &= ~requested[index]; break;
        case 3: mask[index] = requested[index]; break;
        default: return GuestPosixFailure(EINVAL);
        }
    }
    return 0;
}

}
