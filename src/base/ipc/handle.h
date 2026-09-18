// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef BASE_IPC_HANDLE_H
#define BASE_IPC_HANDLE_H

#include <cstdint>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

namespace base {
namespace ipc {

// Owning Windows HANDLE (MojoWrapPlatformHandle). Dtor closes when owns.
struct PlatformHandle {
  HANDLE native = INVALID_HANDLE_VALUE;
  bool owns = true;

  PlatformHandle() = default;
  ~PlatformHandle();
  PlatformHandle(const PlatformHandle&) = delete;
  PlatformHandle& operator=(const PlatformHandle&) = delete;
  PlatformHandle(PlatformHandle&& other) noexcept;
  PlatformHandle& operator=(PlatformHandle&& other) noexcept;

  static PlatformHandle borrow(HANDLE handle);
  static PlatformHandle adopt(HANDLE handle);

  bool is_valid() const;
  void reset();
  HANDLE release();
};

// Duplicate `local` into `target_process`. The result is valid only there.
bool wrap_into(HANDLE local, HANDLE target_process, PlatformHandle* out);

// Token on the wire: HANDLE value as seen by the destination process.
uint64_t handle_to_token(const PlatformHandle& handle);
PlatformHandle handle_from_token(uint64_t token);

}  // namespace ipc
}  // namespace base

#endif  // BASE_IPC_HANDLE_H
