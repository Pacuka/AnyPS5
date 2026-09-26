#ifndef CORE_LIBS_PRX_LIBKERNEL_SOCKET_NETBRIDGE_HPP
#define CORE_LIBS_PRX_LIBKERNEL_SOCKET_NETBRIDGE_HPP

#include <cstddef>
#include <cstdint>

// Host socket bridge shared by the libkernel POSIX socket calls and libSceNet. Guest sockets are
// host descriptors; addresses, options and flags use FreeBSD encodings and are translated here.
// Every function returns a non-negative result or the negated guest (FreeBSD) errno.

extern "C" {

int NetSocket_nid_no_patch(int family, int type, int protocol);
int NetClose_nid_no_patch(int s);
int NetBind_nid_no_patch(int s, const void* addr, std::uint32_t addrlen);
int NetConnect_nid_no_patch(int s, const void* addr, std::uint32_t addrlen);
int NetListen_nid_no_patch(int s, int backlog);
int NetAccept_nid_no_patch(int s, void* addr, std::uint32_t* addrlen);
int NetShutdown_nid_no_patch(int s, int how);
int NetGetsockname_nid_no_patch(int s, void* addr, std::uint32_t* addrlen);
int NetGetpeername_nid_no_patch(int s, void* addr, std::uint32_t* addrlen);
// sceTimeouts selects the libSceNet encoding of SO_SNDTIMEO/SO_RCVTIMEO (int microseconds)
// instead of the POSIX struct timeval.
int NetSetsockopt_nid_no_patch(int s, int level, int name, const void* value, std::uint32_t length, bool sceTimeouts);
int NetGetsockopt_nid_no_patch(int s, int level, int name, void* value, std::uint32_t* length, bool sceTimeouts);
std::int64_t NetSend_nid_no_patch(int s, const void* buf, std::size_t len, int flags, const void* to, std::uint32_t tolen);
std::int64_t NetRecv_nid_no_patch(int s, void* buf, std::size_t len, int flags, void* from, std::uint32_t* fromlen);
int NetInetPton_nid_no_patch(int af, const char* src, void* dst);
const char* NetInetNtop_nid_no_patch(int af, const void* src, char* dst, std::uint32_t size, int* error);
// Converts a guest sockaddr to a host one and back; used by the epoll and resolver paths.
int NetGuestFamilyToHost_nid_no_patch(int family);
int NetHostFamilyToGuest_nid_no_patch(int family);

}

#endif
