#include <algorithm>
#include <cerrno>
#include <cstdint>
#include <cstddef>
#include <cstring>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <vector>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/GuestErrno.hpp"
#include "prx/libkernel/File/include/File.hpp"
#include "prx/libkernel/File/include/NativeStat.hpp"

#ifdef _WIN32
#include <io.h>
#include <sys/utime.h>
#else
#include <dirent.h>
#include <fcntl.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/time.h>
#include <unistd.h>
#endif

namespace {

// POSIX entry points report failures through the guest errno; sce* results carry it in the low bits.
std::int64_t PosixResult(std::int64_t sceResult) {
    if (sceResult >= 0) return sceResult;
    errno = static_cast<int>(static_cast<std::uint64_t>(sceResult) & 0xFFFFu);
    return -1;
}

int SceResult(int nativeResult) {
    return nativeResult == 0 ? 0 : SceKernelError(errno);
}

std::filesystem::path Resolve(const char* path, const char* function) {
    if (!path) throw std::invalid_argument(std::string(function) + ": path is null");
    return ResolvePath_nid_no_patch(path);
}

#ifdef _WIN32
[[noreturn]] void Unsupported(const char* function) {
    throw std::runtime_error(std::string(function) + " is not implemented on Windows yet");
}
#endif

// FreeBSD struct dirent as the guest sees it: 32-bit inode, 4-byte aligned records.
struct GuestDirentHeader {
    std::uint32_t d_fileno;
    std::uint16_t d_reclen;
    std::uint8_t d_type;
    std::uint8_t d_namlen;
};
constexpr std::size_t GUEST_DIRENT_NAME_MAX = 255;

std::size_t GuestDirentSize(std::size_t nameLength) {
    return (sizeof(GuestDirentHeader) + nameLength + 1 + 3) & ~std::size_t(3);
}

int ReadDirectory(int fd, char* buf, int nbytes, std::int64_t* basep) {
#ifdef _WIN32
    (void)fd; (void)buf; (void)nbytes; (void)basep;
    Unsupported("sceKernelGetdents");
#else
    if (!buf || nbytes < 0) return SceKernelError(EINVAL);
    const off_t start = ::lseek(fd, 0, SEEK_CUR);
    if (start < 0) return SceKernelError(errno);
    if (basep) *basep = start;
    struct LinuxDirent64 {
        std::uint64_t d_ino;
        std::int64_t d_off;
        std::uint16_t d_reclen;
        std::uint8_t d_type;
        char d_name[1];
    };
    std::vector<char> native(static_cast<std::size_t>(std::max(nbytes, 512)) * 2);
    const long read = ::syscall(SYS_getdents64, fd, native.data(), native.size());
    if (read < 0) return SceKernelError(errno);
    std::size_t written = 0;
    off_t resume = start;
    for (long offset = 0; offset < read;) {
        const auto* entry = reinterpret_cast<const LinuxDirent64*>(native.data() + offset);
        const std::size_t nameLength = std::min(std::strlen(entry->d_name), GUEST_DIRENT_NAME_MAX);
        const std::size_t size = GuestDirentSize(nameLength);
        if (written + size > static_cast<std::size_t>(nbytes)) break;
        GuestDirentHeader header{static_cast<std::uint32_t>(entry->d_ino), static_cast<std::uint16_t>(size), entry->d_type, static_cast<std::uint8_t>(nameLength)};
        std::memset(buf + written, 0, size);
        std::memcpy(buf + written, &header, sizeof(header));
        std::memcpy(buf + written + sizeof(header), entry->d_name, nameLength);
        written += size;
        resume = entry->d_off;
        offset += entry->d_reclen;
    }
    if (written == 0 && read > 0) return SceKernelError(EINVAL);
    // Entries that did not fit are returned by the next call.
    if (::lseek(fd, resume, SEEK_SET) < 0) return SceKernelError(errno);
    return static_cast<int>(written);
#endif
}

std::int64_t PositionalRead(int d, void* buf, size_t nbytes, int64_t offset) {
    if (!buf && nbytes != 0) return SceKernelError(EFAULT);
    if (offset < 0) return SceKernelError(EINVAL);
#ifdef _WIN32
    (void)d;
    Unsupported("sceKernelPread");
#else
    const ssize_t result = ::pread(d, buf, nbytes, static_cast<off_t>(offset));
    return result < 0 ? SceKernelError(errno) : result;
#endif
}

std::int64_t PositionalWrite(int d, const void* buf, size_t nbytes, int64_t offset) {
    if (!buf && nbytes != 0) return SceKernelError(EFAULT);
    if (offset < 0) return SceKernelError(EINVAL);
#ifdef _WIN32
    (void)d;
    Unsupported("sceKernelPwrite");
#else
    const ssize_t result = ::pwrite(d, buf, nbytes, static_cast<off_t>(offset));
    return result < 0 ? SceKernelError(errno) : result;
#endif
}

int Truncate(int d, int64_t length) {
    if (length < 0) return SceKernelError(EINVAL);
#ifdef _WIN32
    return SceResult(::_chsize_s(d, length) == 0 ? 0 : -1);
#else
    return SceResult(::ftruncate(d, static_cast<off_t>(length)));
#endif
}

int ChangeMode(const char* path, int mode) {
    const auto native = Resolve(path, "chmod");
#ifdef _WIN32
    return SceResult(::_wchmod(native.wstring().c_str(), mode & 0600));
#else
    return SceResult(::chmod(native.c_str(), static_cast<mode_t>(mode)));
#endif
}

int MakeDirectory(const char* path, uint16_t mode) {
    const auto native = Resolve(path, "mkdir");
    std::error_code error;
    if (std::filesystem::exists(native, error)) return SceKernelError(EEXIST);
    if (!std::filesystem::create_directory(native, error)) return SceKernelError(error ? error.value() : EEXIST);
#ifndef _WIN32
    ::chmod(native.c_str(), static_cast<mode_t>(mode));
#else
    (void)mode;
#endif
    return 0;
}

int StatPath(const char* path, FileStat* sb) {
    if (!sb) return SceKernelError(EFAULT);
    const int error = File::FillFileStat(Resolve(path, "stat"), sb);
    return error == 0 ? 0 : SceKernelError(error);
}

}

extern "C" {

int APS5_VABI chmod_nid_postfix(const char* path, int mode) {
    return static_cast<int>(PosixResult(ChangeMode(path, mode)));
}

int APS5_VABI close_nid_postfix(int d) {
#ifdef _WIN32
    return ::_close(d) == 0 ? 0 : GuestPosixFailure(errno);
#else
    return ::close(d) == 0 ? 0 : GuestPosixFailure(errno);
#endif
}

int APS5_VABI flock_nid_postfix(int d, int operation) {
#ifdef _WIN32
    (void)d; (void)operation;
    Unsupported(__func__);
#else
    // FreeBSD and Linux share the LOCK_SH/LOCK_EX/LOCK_NB/LOCK_UN values.
    return ::flock(d, operation) == 0 ? 0 : GuestPosixFailure(errno);
#endif
}

int64_t APS5_VABI fstat_nid_disambig1_nid_postfix(int d, FileStat* sb) {
    if (!sb) return GuestPosixFailure(EFAULT);
    const int error = File::FillFileStat(d, sb);
    return error == 0 ? 0 : GuestPosixFailure(error);
}

int APS5_VABI ftruncate_nid_postfix(int d, int64_t length) {
    return static_cast<int>(PosixResult(Truncate(d, length)));
}

int64_t APS5_VABI lseek_nid_postfix(int d, int64_t offset, int whence) {
    if (whence < 0 || whence > 2) return GuestPosixFailure(EINVAL);
#ifdef _WIN32
    const std::int64_t result = ::_lseeki64(d, offset, whence);
#else
    const std::int64_t result = ::lseek(d, static_cast<off_t>(offset), whence);
#endif
    return result < 0 ? GuestPosixFailure(errno) : result;
}

int APS5_VABI mkdir_nid_postfix(const char* path, uint16_t mode) {
    return static_cast<int>(PosixResult(MakeDirectory(path, mode)));
}

int APS5_VABI open_nid_postfix(const char* path, int flags, int mode) {
    return static_cast<int>(PosixResult(sceKernelOpen(path, flags, static_cast<std::uint16_t>(mode))));
}

int64_t APS5_VABI pread_nid_postfix(int d, void* buf, size_t nbytes, int64_t offset) {
    return PosixResult(PositionalRead(d, buf, nbytes, offset));
}

int64_t APS5_VABI pwrite_nid_disambig1_nid_postfix(int d, const void* buf, size_t nbytes, int64_t offset) {
    return PosixResult(PositionalWrite(d, buf, nbytes, offset));
}

int64_t APS5_VABI read_nid_postfix(int d, void* buf, uint64_t nbytes) {
    if (!buf && nbytes != 0) return GuestPosixFailure(EFAULT);
#ifdef _WIN32
    const std::int64_t result = ::_read(d, buf, static_cast<unsigned>(std::min<uint64_t>(nbytes, 0x7FFFFFFF)));
#else
    const std::int64_t result = ::read(d, buf, nbytes);
#endif
    return result < 0 ? GuestPosixFailure(errno) : result;
}

int64_t APS5_VABI write_nid_postfix(int d, const char* str, int64_t size) {
    if (size < 0) return GuestPosixFailure(EINVAL);
    if (!str && size != 0) return GuestPosixFailure(EFAULT);
#ifdef _WIN32
    const std::int64_t result = ::_write(d, str, static_cast<unsigned>(std::min<int64_t>(size, 0x7FFFFFFF)));
#else
    const std::int64_t result = ::write(d, str, static_cast<size_t>(size));
#endif
    return result < 0 ? GuestPosixFailure(errno) : result;
}

int APS5_VABI stat_nid_postfix(const char* path, FileStat* sb) {
    return static_cast<int>(PosixResult(StatPath(path, sb)));
}

int APS5_VABI sceKernelCheckReachability(const char* path) {
    const auto native = Resolve(path, __func__);
    std::error_code error;
    return std::filesystem::exists(native, error) ? 0 : SceKernelError(ENOENT);
}

int APS5_VABI sceKernelFstat(int d, FileStat* sb) {
    if (!sb) return SceKernelError(EFAULT);
    const int error = File::FillFileStat(d, sb);
    return error == 0 ? 0 : SceKernelError(error);
}

int APS5_VABI sceKernelFsync(int fd) {
#ifdef _WIN32
    return SceResult(::_commit(fd));
#else
    return SceResult(::fsync(fd));
#endif
}

int APS5_VABI sceKernelGetdents(int fd, char* buf, int nbytes) {
    return ReadDirectory(fd, buf, nbytes, nullptr);
}

int APS5_VABI sceKernelGetdirentries(int fd, char* buf, int nbytes, int64_t* basep) {
    return ReadDirectory(fd, buf, nbytes, basep);
}

int APS5_VABI sceKernelMkdir(const char* path, uint16_t mode) {
    return MakeDirectory(path, mode);
}

int64_t APS5_VABI sceKernelPread(int d, void* buf, size_t nbytes, int64_t offset) {
    return PositionalRead(d, buf, nbytes, offset);
}

int64_t APS5_VABI sceKernelPwrite(int d, const void* buf, size_t nbytes, int64_t offset) {
    return PositionalWrite(d, buf, nbytes, offset);
}

int APS5_VABI sceKernelRename(const char* from, const char* to) {
    const auto source = Resolve(from, __func__);
    const auto target = Resolve(to, __func__);
    std::error_code error;
    std::filesystem::rename(source, target, error);
    return error ? SceKernelError(error.value()) : 0;
}

int APS5_VABI sceKernelRmdir(const char* path) {
    const auto native = Resolve(path, __func__);
    std::error_code error;
    if (!std::filesystem::is_directory(native, error)) return SceKernelError(std::filesystem::exists(native, error) ? ENOTDIR : ENOENT);
    if (!std::filesystem::is_empty(native, error)) return SceKernelError(ENOTEMPTY);
    std::filesystem::remove(native, error);
    return error ? SceKernelError(error.value()) : 0;
}

int APS5_VABI sceKernelChmod(const char* path, uint16_t mode) {
    return ChangeMode(path, mode);
}

int APS5_VABI sceKernelFtruncate(int fd, int64_t length) {
    return Truncate(fd, length);
}

int APS5_VABI sceKernelUtimes(const char* path, const KernelTimeval* times) {
    const auto native = Resolve(path, __func__);
#ifdef _WIN32
    (void)native; (void)times;
    Unsupported(__func__);
#else
    if (!times) return SceResult(::utimes(native.c_str(), nullptr));
    timeval host[2]{};
    for (int index = 0; index < 2; ++index) {
        host[index].tv_sec = static_cast<time_t>(times[index].tv_sec);
        host[index].tv_usec = static_cast<suseconds_t>(times[index].tv_usec);
    }
    return SceResult(::utimes(native.c_str(), host));
#endif
}


}
