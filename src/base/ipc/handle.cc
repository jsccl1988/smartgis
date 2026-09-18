// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "base/ipc/handle.h"

namespace base {
namespace ipc {

PlatformHandle::~PlatformHandle() {
  reset();
}

PlatformHandle::PlatformHandle(PlatformHandle&& other) noexcept
    : native(other.native), owns(other.owns) {
  other.native = INVALID_HANDLE_VALUE;
  other.owns = false;
}

PlatformHandle& PlatformHandle::operator=(PlatformHandle&& other) noexcept {
  if (this != &other) {
    reset();
    native = other.native;
    owns = other.owns;
    other.native = INVALID_HANDLE_VALUE;
    other.owns = false;
  }
  return *this;
}

PlatformHandle PlatformHandle::borrow(HANDLE handle) {
  PlatformHandle h;
  h.native = handle;
  h.owns = false;
  return h;
}

PlatformHandle PlatformHandle::adopt(HANDLE handle) {
  PlatformHandle h;
  h.native = handle;
  h.owns = true;
  return h;
}

bool PlatformHandle::is_valid() const {
  return native != nullptr && native != INVALID_HANDLE_VALUE;
}

HANDLE PlatformHandle::release() {
  const HANDLE h = native;
  native = INVALID_HANDLE_VALUE;
  owns = false;
  return h;
}

void PlatformHandle::reset() {
  if (owns && is_valid()) {
    CloseHandle(native);
  }
  native = INVALID_HANDLE_VALUE;
  owns = true;
}

bool wrap_into(HANDLE local, HANDLE target_process, PlatformHandle* out) {
  if (!out) {
    return false;
  }
  out->reset();
  if (!local || local == INVALID_HANDLE_VALUE || !target_process) {
    return false;
  }
  HANDLE remote = nullptr;
  if (!DuplicateHandle(GetCurrentProcess(), local, target_process, &remote, 0,
                       FALSE, DUPLICATE_SAME_ACCESS)) {
    return false;
  }
  *out = PlatformHandle::adopt(remote);
  return true;
}

uint64_t handle_to_token(const PlatformHandle& handle) {
  return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(handle.native));
}

PlatformHandle handle_from_token(uint64_t token) {
  return PlatformHandle::adopt(
      reinterpret_cast<HANDLE>(static_cast<uintptr_t>(token)));
}

}  // namespace ipc
}  // namespace base
