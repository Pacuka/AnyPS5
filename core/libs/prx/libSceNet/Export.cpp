#include <atomic>
#include <cerrno>
#include <cstdint>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <map>
#include <mutex>
#include <stdexcept>
#include <string>
#include <utility>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/GuestErrno.hpp"
#include "prx/libkernel/Socket/include/NetBridge.hpp"

#ifndef _WIN32
#include <arpa/inet.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <netdb.h>
#include <netpacket/packet.h>
#include <sys/epoll.h>
#include <sys/ioctl.h>
#include <linux/sockios.h>
#include <unistd.h>
#endif

namespace {

constexpr int SCE_NET_ERROR_BASE = static_cast<int>(0x80410100);
constexpr int SCE_NET_ERROR_RESOLVER_ENOHOST = static_cast<int>(0x804101EA);
constexpr int SCE_NET_EPOLL_CTL_ADD = 1;
constexpr int SCE_NET_EPOLL_CTL_MOD = 2;
constexpr int SCE_NET_EPOLL_CTL_DEL = 3;
constexpr std::uint32_t SCE_NET_EPOLLIN = 0x1;
constexpr std::uint32_t SCE_NET_EPOLLOUT = 0x2;
constexpr std::uint32_t SCE_NET_EPOLLERR = 0x8;
constexpr std::uint32_t SCE_NET_EPOLLHUP = 0x10;
constexpr int GUEST_AF_INET = 2;
constexpr int RESOLVER_MAX_RECORDS = 16;

thread_local int netErrno = 0;
std::atomic<int> nextPoolId{1};
std::atomic<int> nextResolverId{1};

// libSceNet reports failures both as SCE_NET_ERROR_* codes and through sceNetErrnoLoc().
int netResult(std::int64_t result) {
    if (result >= 0) return static_cast<int>(result);
    netErrno = static_cast<int>(-result);
    return SCE_NET_ERROR_BASE | netErrno;
}

int netError(int hostError) {
    return netResult(-GuestErrno(hostError));
}

struct GuestSockInfo {
    char name[32];
    std::int32_t socketId;
    std::int32_t policy;
    std::int32_t priority;
    std::int32_t recvQueueLength;
    std::int32_t sendQueueLength;
    std::uint32_t localAddress;
    std::uint32_t remoteAddress;
    std::uint16_t localPort;
    std::uint16_t remotePort;
    std::uint16_t localVport;
    std::uint16_t remoteVport;
};
static_assert(offsetof(GuestSockInfo, recvQueueLength) == 0x2C);

struct GuestSockaddrIn {
    std::uint8_t length;
    std::uint8_t family;
    std::uint16_t port;
    std::uint32_t address;
    std::uint8_t zero[8];
};

struct ResolverAddress {
    std::uint8_t address[16];
    std::int32_t family;
};

struct ResolverInfo {
    ResolverAddress addresses[RESOLVER_MAX_RECORDS];
    std::uint32_t records;
    std::uint32_t dns4records;
    std::uint32_t reserved[14];
};

#ifndef _WIN32

// Guest data registered for each socket of an epoll instance.
std::mutex epollLock;
std::map<std::pair<int, int>, NetEpollData> epollData;

std::uint32_t toHostEpollEvents(std::uint32_t events) {
    std::uint32_t host = 0;
    if (events & SCE_NET_EPOLLIN) host |= EPOLLIN;
    if (events & SCE_NET_EPOLLOUT) host |= EPOLLOUT;
    if (events & SCE_NET_EPOLLERR) host |= EPOLLERR;
    if (events & SCE_NET_EPOLLHUP) host |= EPOLLHUP;
    return host;
}

std::uint32_t toGuestEpollEvents(std::uint32_t events) {
    std::uint32_t guest = 0;
    if (events & EPOLLIN) guest |= SCE_NET_EPOLLIN;
    if (events & EPOLLOUT) guest |= SCE_NET_EPOLLOUT;
    if (events & EPOLLERR) guest |= SCE_NET_EPOLLERR;
    if (events & (EPOLLHUP | EPOLLRDHUP)) guest |= SCE_NET_EPOLLHUP;
    return guest;
}

int resolveIpv4(const char* hostname, std::uint32_t* addresses, int capacity) {
    addrinfo hints{};
    hints.ai_family = AF_INET;
    addrinfo* results = nullptr;
    if (::getaddrinfo(hostname, nullptr, &hints, &results) != 0 || !results) return 0;
    int count = 0;
    for (const addrinfo* entry = results; entry && count < capacity; entry = entry->ai_next) {
        addresses[count++] = reinterpret_cast<const sockaddr_in*>(entry->ai_addr)->sin_addr.s_addr;
    }
    ::freeaddrinfo(results);
    return count;
}

#endif

}

extern "C" {

int APS5_VABI sceNetInit_nid_postfix(void) {
    return 0;
}

int* APS5_VABI sceNetErrnoLoc_nid_postfix() {
    return &netErrno;
}

int APS5_VABI sceNetPoolCreate(const char* name, int size, int flags) {
    (void)name;
    (void)flags;
    if (size <= 0) return netError(EINVAL);
    return nextPoolId++;
}

int APS5_VABI sceNetPoolDestroy(int memid) {
    return memid > 0 ? 0 : netError(EINVAL);
}

uint32_t APS5_VABI sceNetHtonl_nid_postfix(uint32_t host32) {
    return __builtin_bswap32(host32);
}

uint16_t APS5_VABI sceNetHtons_nid_postfix(uint16_t host16) {
    return __builtin_bswap16(host16);
}

uint32_t APS5_VABI sceNetNtohl_nid_postfix(uint32_t net32) {
    return __builtin_bswap32(net32);
}

uint16_t APS5_VABI sceNetNtohs_nid_postfix(uint16_t net16) {
    return __builtin_bswap16(net16);
}

const char* APS5_VABI sceNetInetNtop(int af, const void* src, char* dst, uint32_t size) {
    int error = 0;
    const char* result = NetInetNtop_nid_no_patch(af, src, dst, size, &error);
    if (!result) netErrno = error;
    return result;
}

int APS5_VABI sceNetInetPton(int af, const char* src, void* dst) {
    return netResult(NetInetPton_nid_no_patch(af, src, dst));
}

int APS5_VABI sceNetEtherNtostr(const NetEtherAddr* n, char* str, size_t len) {
    if (!n || !str) return netError(EINVAL);
    if (len < 18) return netError(ENOSPC);
    std::snprintf(str, len, "%02x:%02x:%02x:%02x:%02x:%02x", n->data[0], n->data[1], n->data[2], n->data[3], n->data[4], n->data[5]);
    return 0;
}

int APS5_VABI sceNetGetMacAddress(NetEtherAddr* addr, int flags) {
    (void)flags;
    if (!addr) return netError(EINVAL);
    std::memset(addr->data, 0, sizeof(addr->data));
#ifndef _WIN32
    ifaddrs* interfaces = nullptr;
    if (::getifaddrs(&interfaces) == 0) {
        for (const ifaddrs* entry = interfaces; entry; entry = entry->ifa_next) {
            if (!entry->ifa_addr || entry->ifa_addr->sa_family != AF_PACKET || (entry->ifa_flags & IFF_LOOPBACK)) continue;
            const auto* link = reinterpret_cast<const sockaddr_ll*>(entry->ifa_addr);
            if (link->sll_halen != sizeof(addr->data)) continue;
            std::memcpy(addr->data, link->sll_addr, sizeof(addr->data));
            break;
        }
        ::freeifaddrs(interfaces);
    }
#endif
    return 0;
}

int APS5_VABI sceNetSocket(const char* name, int family, int type, int protocol) {
    (void)name;
    return netResult(NetSocket_nid_no_patch(family, type, protocol));
}

int APS5_VABI sceNetSocketClose(int s) {
    return netResult(NetClose_nid_no_patch(s));
}

int APS5_VABI sceNetBind_nid_postfix(int s, const void* addr, uint32_t addrlen) {
    return netResult(NetBind_nid_no_patch(s, addr, addrlen));
}

int APS5_VABI sceNetConnect_nid_postfix(int s, const void* addr, uint32_t addrlen) {
    return netResult(NetConnect_nid_no_patch(s, addr, addrlen));
}

int APS5_VABI sceNetListen(int s, int backlog) {
    return netResult(NetListen_nid_no_patch(s, backlog));
}

int APS5_VABI sceNetAccept(int s, void* addr, uint32_t* addrlen) {
    return netResult(NetAccept_nid_no_patch(s, addr, addrlen));
}

int APS5_VABI sceNetShutdown(int s, int how) {
    return netResult(NetShutdown_nid_no_patch(s, how));
}

int APS5_VABI sceNetGetsockname(int s, void* addr, uint32_t* addrlen) {
    return netResult(NetGetsockname_nid_no_patch(s, addr, addrlen));
}

int APS5_VABI sceNetSetsockopt(int s, int level, int optname, const void* optval, uint32_t optlen) {
    return netResult(NetSetsockopt_nid_no_patch(s, level, optname, optval, optlen, true));
}

int APS5_VABI sceNetSend_nid_postfix(int s, const void* buf, size_t len, int flags) {
    return netResult(NetSend_nid_no_patch(s, buf, len, flags, nullptr, 0));
}

int APS5_VABI sceNetSendto_nid_postfix(int s, const void* buf, size_t len, int flags, const void* addr, uint32_t addrlen) {
    return netResult(NetSend_nid_no_patch(s, buf, len, flags, addr, addrlen));
}

int APS5_VABI sceNetRecv_nid_postfix(int s, void* buf, size_t len, int flags) {
    return netResult(NetRecv_nid_no_patch(s, buf, len, flags, nullptr, nullptr));
}

int APS5_VABI sceNetRecvfrom_nid_postfix(int s, void* buf, size_t len, int flags, void* addr, uint32_t* addrlen) {
    return netResult(NetRecv_nid_no_patch(s, buf, len, flags, addr, addrlen));
}

// Fills one entry for the given socket; only IPv4 endpoints are reported.
int APS5_VABI sceNetGetSockInfo(int s, void* info, int n, int flags) {
    (void)flags;
    if (!info || n < 1 || s < 0) return netError(EINVAL);
    GuestSockInfo entry{};
    entry.socketId = s;
#ifdef _WIN32
    throw std::runtime_error("sceNetGetSockInfo is not implemented on Windows yet");
#else
    int pending = 0;
    if (::ioctl(s, FIONREAD, &pending) != 0) return netError(errno);
    entry.recvQueueLength = pending;
    if (::ioctl(s, SIOCOUTQ, &pending) == 0) entry.sendQueueLength = pending;
    GuestSockaddrIn address{};
    std::uint32_t length = sizeof(address);
    if (NetGetsockname_nid_no_patch(s, &address, &length) == 0 && address.family == GUEST_AF_INET) {
        entry.localAddress = address.address;
        entry.localPort = address.port;
    }
    length = sizeof(address);
    if (NetGetpeername_nid_no_patch(s, &address, &length) == 0 && address.family == GUEST_AF_INET) {
        entry.remoteAddress = address.address;
        entry.remotePort = address.port;
    }
#endif
    std::memcpy(info, &entry, sizeof(entry));
    return 1;
}

int APS5_VABI sceNetEpollCreate(const char* name, int flags) {
    (void)name;
    (void)flags;
#ifdef _WIN32
    throw std::runtime_error("sceNetEpollCreate is not implemented on Windows yet");
#else
    const int eid = ::epoll_create1(EPOLL_CLOEXEC);
    return eid < 0 ? netError(errno) : eid;
#endif
}

int APS5_VABI sceNetEpollDestroy(int eid) {
#ifdef _WIN32
    (void)eid;
    throw std::runtime_error("sceNetEpollDestroy is not implemented on Windows yet");
#else
    {
        std::lock_guard lock(epollLock);
        for (auto it = epollData.lower_bound({eid, 0}); it != epollData.end() && it->first.first == eid;) it = epollData.erase(it);
    }
    return ::close(eid) == 0 ? 0 : netError(errno);
#endif
}

int APS5_VABI sceNetEpollControl(int eid, int op, int id, const NetEpollEvent* event) {
#ifdef _WIN32
    (void)eid; (void)op; (void)id; (void)event;
    throw std::runtime_error("sceNetEpollControl is not implemented on Windows yet");
#else
    int hostOp = 0;
    switch (op) {
    case SCE_NET_EPOLL_CTL_ADD: hostOp = EPOLL_CTL_ADD; break;
    case SCE_NET_EPOLL_CTL_MOD: hostOp = EPOLL_CTL_MOD; break;
    case SCE_NET_EPOLL_CTL_DEL: hostOp = EPOLL_CTL_DEL; break;
    default: return netError(EINVAL);
    }
    if (hostOp != EPOLL_CTL_DEL && !event) return netError(EINVAL);
    epoll_event host{};
    host.events = event ? toHostEpollEvents(event->events) : 0;
    host.data.fd = id;
    if (::epoll_ctl(eid, hostOp, id, &host) != 0) return netError(errno);
    std::lock_guard lock(epollLock);
    if (hostOp == EPOLL_CTL_DEL) epollData.erase({eid, id});
    else epollData[{eid, id}] = event->data;
    return 0;
#endif
}

// The timeout is in microseconds; a negative value waits indefinitely.
int APS5_VABI sceNetEpollWait(int eid, NetEpollEvent* events, int maxevents, int timeout) {
#ifdef _WIN32
    (void)eid; (void)events; (void)maxevents; (void)timeout;
    throw std::runtime_error("sceNetEpollWait is not implemented on Windows yet");
#else
    if (!events || maxevents <= 0) return netError(EINVAL);
    epoll_event host[64];
    const int capacity = maxevents < 64 ? maxevents : 64;
    const int milliseconds = timeout < 0 ? -1 : (timeout + 999) / 1000;
    int ready;
    do {
        ready = ::epoll_wait(eid, host, capacity, milliseconds);
    } while (ready < 0 && errno == EINTR);
    if (ready < 0) return netError(errno);
    std::lock_guard lock(epollLock);
    for (int index = 0; index < ready; ++index) {
        const int id = host[index].data.fd;
        events[index] = NetEpollEvent{};
        events[index].events = toGuestEpollEvents(host[index].events);
        events[index].ident = static_cast<std::uint64_t>(id);
        const auto found = epollData.find({eid, id});
        if (found != epollData.end()) events[index].data = found->second;
    }
    return ready;
#endif
}

int APS5_VABI sceNetResolverCreate(const char* name, int memid, int flags) {
    (void)name;
    (void)memid;
    (void)flags;
    return nextResolverId++;
}

int APS5_VABI sceNetResolverDestroy_nid_postfix(int rid) {
    return rid > 0 ? 0 : netError(EINVAL);
}

int APS5_VABI sceNetResolverStartNtoa(int rid, const char* hostname, void* addr, int timeout, int retry, int flags) {
    (void)timeout;
    (void)retry;
    (void)flags;
    if (rid <= 0 || !hostname || !addr) return netError(EINVAL);
#ifdef _WIN32
    throw std::runtime_error("sceNetResolverStartNtoa is not implemented on Windows yet");
#else
    std::uint32_t address = 0;
    if (resolveIpv4(hostname, &address, 1) == 0) return SCE_NET_ERROR_RESOLVER_ENOHOST;
    std::memcpy(addr, &address, sizeof(address));
    return 0;
#endif
}

int APS5_VABI sceNetResolverStartNtoaMultipleRecords_nid_postfix(int rid, const char* hostname, ResolverInfo* info, int timeout, int retry, int flags) {
    (void)timeout;
    (void)retry;
    (void)flags;
    if (rid <= 0 || !hostname || !info) return netError(EINVAL);
#ifdef _WIN32
    throw std::runtime_error("sceNetResolverStartNtoaMultipleRecords is not implemented on Windows yet");
#else
    std::uint32_t addresses[RESOLVER_MAX_RECORDS];
    const int count = resolveIpv4(hostname, addresses, RESOLVER_MAX_RECORDS);
    if (count == 0) return SCE_NET_ERROR_RESOLVER_ENOHOST;
    std::memset(info, 0, sizeof(*info));
    for (int index = 0; index < count; ++index) {
        std::memcpy(info->addresses[index].address, &addresses[index], sizeof(addresses[index]));
        info->addresses[index].family = GUEST_AF_INET;
    }
    info->records = static_cast<std::uint32_t>(count);
    info->dns4records = static_cast<std::uint32_t>(count);
    return 0;
#endif
}


}
