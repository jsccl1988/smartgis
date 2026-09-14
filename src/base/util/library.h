// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef BASE_UTIL_LIBRARY_H_
#define BASE_UTIL_LIBRARY_H_

#include <string>
#include <utility>

#include "base/core/build_config.h"
#include "base/core/log.h"
#include "base/core/macros.h"
#include "base/util/path.h"

#if defined(OS_WIN)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#else
#error base::library requires Windows (LoadLibraryW) in this tree
#endif

namespace base {

// Dynamic library loader (Windows: LoadLibraryW / GetProcAddress / FreeLibrary).
class library {
 public:
  explicit library(path filename = {})
      : filename_(std::move(filename)), native_handle_(nullptr) {}

  library(library&& other) noexcept
      : filename_(std::move(other.filename_)),
        native_handle_(other.native_handle_),
        last_error_(std::move(other.last_error_)) {
    other.native_handle_ = nullptr;
  }

  library& operator=(library&& other) noexcept {
    if (this != &other) {
      unload();
      filename_ = std::move(other.filename_);
      native_handle_ = other.native_handle_;
      last_error_ = std::move(other.last_error_);
      other.native_handle_ = nullptr;
    }
    return *this;
  }

  ~library() { unload(); }

  DISALLOW_COPY_AND_ASSIGN(library);

  bool load(path filename = {}) {
    if (!filename.empty()) {
      filename_ = std::move(filename);
    }
    if (filename_.empty()) {
      last_error_ = "No filename provided";
      LOGGING(LOG_ERROR, "Library load failed: %s", last_error_.c_str());
      return false;
    }
    if (native_handle_) {
      return true;
    }
    std::wstring wpath = filename_.wstring();
    native_handle_ = ::LoadLibraryW(wpath.c_str());
    if (!native_handle_) {
      last_error_ = "LoadLibraryW failed";
      LOGGING(LOG_ERROR, "Failed to load library: %s", filename_.string().c_str());
      return false;
    }
    return true;
  }

  bool unload() {
    if (!native_handle_) {
      return true;
    }
    BOOL ok = ::FreeLibrary(native_handle_);
    native_handle_ = nullptr;
    if (!ok) {
      last_error_ = "FreeLibrary failed";
      return false;
    }
    return true;
  }

  bool is_loaded() const { return native_handle_ != nullptr; }
  const path& get_filename() const { return filename_; }
  const std::string& last_error() const { return last_error_; }
  HMODULE native_handle() const { return native_handle_; }

  template <typename Fn>
  Fn* resolve(const char* symbol) const {
    if (!native_handle_ || !symbol) {
      return nullptr;
    }
    return reinterpret_cast<Fn*>(::GetProcAddress(native_handle_, symbol));
  }

 private:
  path filename_;
  HMODULE native_handle_;
  std::string last_error_;
};

}  // namespace base

#endif  // BASE_UTIL_LIBRARY_H_
