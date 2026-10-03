#include <rex/platform.h>
#include <rex/platform/dynlib.h>

static_assert(REX_PLATFORM_LINUX || REX_PLATFORM_MAC, "This file is POSIX-only");

#include <dlfcn.h>

#include <rex/filesystem.h>

namespace rex::platform {

DynamicLibrary::~DynamicLibrary() {
  Close();
}

DynamicLibrary::DynamicLibrary(DynamicLibrary&& other) noexcept : handle_(other.handle_) {
  other.handle_ = nullptr;
}

DynamicLibrary& DynamicLibrary::operator=(DynamicLibrary&& other) noexcept {
  if (this != &other) {
    Close();
    handle_ = other.handle_;
    other.handle_ = nullptr;
  }
  return *this;
}

bool DynamicLibrary::Load(const std::filesystem::path& path, SymbolResolution mode) {
  Close();
  int flags = (mode == SymbolResolution::kImmediate) ? RTLD_NOW : RTLD_LAZY;
  handle_ = dlopen(path.c_str(), flags);
  if (handle_ || path.has_parent_path() || path.has_extension()) {
    return handle_ != nullptr;
  }
  // A bare module name: dlopen neither adds the suffix nor searches beside
  // the executable, as LoadLibrary does.
  const std::string stem = path.string();
#if REX_PLATFORM_MAC
  const char* suffix = ".dylib";
#else
  const char* suffix = ".so";
#endif
  const std::filesystem::path exe_dir = rex::filesystem::GetExecutableFolder();
  const std::filesystem::path candidates[] = {
      exe_dir / ("lib" + stem + suffix),
      exe_dir / (stem + suffix),
      std::filesystem::path("lib" + stem + suffix),
      std::filesystem::path(stem + suffix),
  };
  for (const auto& candidate : candidates) {
    handle_ = dlopen(candidate.c_str(), flags);
    if (handle_) {
      return true;
    }
  }
  return false;
}

void DynamicLibrary::Close() {
  if (handle_) {
    dlclose(handle_);
    handle_ = nullptr;
  }
}

void* DynamicLibrary::GetRawSymbol(const char* name) const {
  if (!handle_)
    return nullptr;
  return dlsym(handle_, name);
}

}  // namespace rex::platform
