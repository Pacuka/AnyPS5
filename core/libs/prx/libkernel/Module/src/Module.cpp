#include <cstdio>
#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

#include <mutex>
#include <string>
#include <vector>
#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif

namespace {

constexpr KernelModule SCE_KERNEL_ERROR_ENOENT_MODULE = static_cast<KernelModule>(0x80020002u);
constexpr KernelModule SCE_KERNEL_ERROR_EINVAL_MODULE = static_cast<KernelModule>(0x80020016u);

std::mutex g_moduleMutex;
std::vector<void*> g_modules;

}

extern "C" {

int APS5_VABI sceKernelDlsym(KernelModule handle, const char* symbol, void** addr) {
 (void)handle;
 (void)symbol;
 (void)addr;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceKernelGetModuleInfoForUnwind(uint64_t addr, int flags, ModuleInfoForUnwind* info) {
 (void)addr;
 (void)flags;
 (void)info;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceKernelGetModuleInfoFromAddr(uint64_t addr, int n, ModuleInfo* r) {
 (void)addr;
 (void)n;
 (void)r;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

KernelModule APS5_VABI sceKernelLoadStartModule(const char* module_file_name, size_t args, const void* argp, uint32_t flags, const KernelLoadModuleOpt* opt, int* res) {
 (void)args;
 (void)argp;
 (void)flags;
 (void)opt;
 if (module_file_name == nullptr) return SCE_KERNEL_ERROR_EINVAL_MODULE;
 // Title modules are relinked ahead of time and loaded as dependencies of the executable, so
 // their initializers have run; loading one hands out a handle to the resident library.
 std::string name(module_file_name);
 const auto slash = name.find_last_of("/\\");
 if (slash != std::string::npos) name.erase(0, slash + 1);
 void* library = nullptr;
#ifdef _WIN32
 auto dllName = name;
 if (dllName.size() > 4 && dllName.compare(dllName.size() - 4, 4, ".prx") == 0) dllName.replace(dllName.size() - 4, 4, ".dll");
 library = reinterpret_cast<void*>(GetModuleHandleA(dllName.c_str()));
#else
 library = dlopen(name.c_str(), RTLD_NOW | RTLD_NOLOAD);
#endif
 if (library == nullptr) {
  std::fprintf(stderr, "[module] %s is not resident\n", module_file_name);
  return SCE_KERNEL_ERROR_ENOENT_MODULE;
 }
 std::lock_guard lock(g_moduleMutex);
 for (std::size_t i = 0; i < g_modules.size(); ++i) {
  if (g_modules[i] == library) {
   if (res) *res = 0;
   return static_cast<KernelModule>(i + 1);
  }
 }
 g_modules.push_back(library);
 if (res) *res = 0;
 return static_cast<KernelModule>(g_modules.size());
}

int APS5_VABI sceKernelStopUnloadModule(KernelModule handle, size_t args, const void* argp, uint32_t flags, const KernelUnloadModuleOpt* opt, int* res) {
 (void)handle;
 (void)args;
 (void)argp;
 (void)flags;
 (void)opt;
 (void)res;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

}
