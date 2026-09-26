#ifndef CORE_LIBS_PRX_LIBKERNEL_FILE_NATIVESTAT_HPP
#define CORE_LIBS_PRX_LIBKERNEL_FILE_NATIVESTAT_HPP

#include <filesystem>
#include "SceTypes.hpp"

namespace File {

// Fill the guest stat of a path or an open descriptor; return 0 or the host errno.
int FillFileStat(const std::filesystem::path& nativePath, FileStat* sb);
int FillFileStat(int fd, FileStat* sb);

}

#endif
