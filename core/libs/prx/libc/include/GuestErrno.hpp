#ifndef CORE_LIBS_PRX_LIBC_INCLUDE_GUESTERRNO_HPP
#define CORE_LIBS_PRX_LIBC_INCLUDE_GUESTERRNO_HPP

#include <cerrno>

// The guest uses FreeBSD errno numbers. Values 1-34 match the host; the rest are translated by name.
inline int GuestErrno(int hostError) {
    if (hostError >= 0 && hostError <= 34) return hostError;
    switch (hostError) {
    case EAGAIN: return 35;
    case EINPROGRESS: return 36;
    case EALREADY: return 37;
    case ENOTSOCK: return 38;
    case EDESTADDRREQ: return 39;
    case EMSGSIZE: return 40;
    case EPROTOTYPE: return 41;
    case ENOPROTOOPT: return 42;
    case EPROTONOSUPPORT: return 43;
    case EOPNOTSUPP: return 45;
    case EAFNOSUPPORT: return 47;
    case EADDRINUSE: return 48;
    case EADDRNOTAVAIL: return 49;
    case ENETDOWN: return 50;
    case ENETUNREACH: return 51;
    case ENETRESET: return 52;
    case ECONNABORTED: return 53;
    case ECONNRESET: return 54;
    case ENOBUFS: return 55;
    case EISCONN: return 56;
    case ENOTCONN: return 57;
    case ETIMEDOUT: return 60;
    case ECONNREFUSED: return 61;
    case ELOOP: return 62;
    case ENAMETOOLONG: return 63;
    case EHOSTUNREACH: return 65;
    case ENOTEMPTY: return 66;
    case EDEADLK: return 11;
    case ENOLCK: return 77;
    case ENOSYS: return 78;
    case EOVERFLOW: return 84;
    case ECANCELED: return 85;
    case EILSEQ: return 86;
    default: return hostError;
    }
}

// Error code returned by sce* kernel functions for a host errno value.
inline int SceKernelError(int hostError) {
    return static_cast<int>(0x80020000u | static_cast<unsigned>(GuestErrno(hostError)));
}

// POSIX-style guest failure: publish the guest errno and return -1.
inline int GuestPosixFailure(int hostError) {
    errno = GuestErrno(hostError);
    return -1;
}

#endif
