// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef APP_VIEWS_UI_PAGES_DETAIL_PTR_GUARD_H_
#define APP_VIEWS_UI_PAGES_DETAIL_PTR_GUARD_H_

#include <cstdint>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

namespace app {
namespace detail {

// Poison / commit-region checks for ViewHost / Workspace pointers after
// partial multi-agent out/Debug rebuilds (skewed vtables look like 0xCD…).

inline bool ptr_addr_poison(uintptr_t addr) {
  if (addr < 0x10000u) {
    return true;
  }
  const auto lo24 = addr & 0xffffff00ull;
  return lo24 == 0xcdcdcd00ull || lo24 == 0xdddddd00ull ||
         lo24 == 0xcccccc00ull || lo24 == 0xfeeefeeeull ||
         lo24 == 0xababab00ull;
}

inline bool ptr_mem_readable(const void* p, size_t nbytes) {
  if (!p || nbytes == 0) {
    return false;
  }
  MEMORY_BASIC_INFORMATION mbi{};
  if (VirtualQuery(p, &mbi, sizeof(mbi)) == 0) {
    return false;
  }
  if (mbi.State != MEM_COMMIT) {
    return false;
  }
  const DWORD prot = mbi.Protect & 0xffu;
  if (prot == PAGE_NOACCESS || prot == PAGE_EXECUTE || prot == PAGE_GUARD) {
    return false;
  }
  const auto* base = static_cast<const uint8_t*>(mbi.BaseAddress);
  const auto* end = static_cast<const uint8_t*>(p) + nbytes;
  return end <= base + mbi.RegionSize;
}

}  // namespace detail
}  // namespace app

#endif  // APP_VIEWS_UI_PAGES_DETAIL_PTR_GUARD_H_
