// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/views/il.runtime/backend/paths.h"

#include <string_view>
#include <utility>

#include "app/views/il.runtime/bind/slots.h"
#include "app/views/util/charset.h"
#include "app/views/util/exe_sidecar_path.h"
#include "app/views/util/find_named.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

namespace app {
namespace detail {
namespace {

// One walk of exe-relative candidates. |canonicalize| runs GetFullPathNameW
// before the encoder; a failed encode tries the next existing file.
template <typename Encode>
bool walk_existing_under_exe(const wchar_t* const* rels,
                             size_t count,
                             bool canonicalize,
                             Encode&& encode) {
  if (!rels || count == 0) {
    return false;
  }
  wchar_t base[MAX_PATH] = {};
  if (!exe_dir_with_slash(base, MAX_PATH)) {
    return false;
  }
  for (size_t i = 0; i < count; ++i) {
    if (!rels[i]) {
      continue;
    }
    wchar_t full[MAX_PATH] = {};
    if (wcscpy_s(full, base) != 0 || wcscat_s(full, rels[i]) != 0) {
      continue;
    }
    if (GetFileAttributesW(full) == INVALID_FILE_ATTRIBUTES) {
      continue;
    }
    const wchar_t* use = full;
    wchar_t canon[MAX_PATH] = {};
    if (canonicalize &&
        GetFullPathNameW(full, MAX_PATH, canon, nullptr) != 0) {
      use = canon;
    }
    if (encode(use)) {
      return true;
    }
  }
  return false;
}

bool copy_utf8(const wchar_t* wide, std::string* out) {
  if (!out || !wide || !wide[0]) {
    return false;
  }
  std::string utf8;
  if (!wide_to_utf8(std::wstring(wide), &utf8) || utf8.empty()) {
    return false;
  }
  *out = std::move(utf8);
  return true;
}

bool resolve_canonical_rels(const wchar_t* const* rels,
                            size_t count,
                            std::string* out) {
  if (!out) {
    return false;
  }
  return walk_existing_under_exe(rels, count, true, [&](const wchar_t* wide) {
    return copy_utf8(wide, out);
  });
}

struct KindRel {
  std::string_view kind;
  const wchar_t* parent_fmt;
  const wchar_t* local_fmt;
};

constexpr KindRel kKindRels[] = {
    {"plugin", L"..\\data\\plugin\\%s", L"data\\plugin\\%s"},
    {"data", L"..\\data\\%s", L"data\\%s"},
};

bool resolve_kind_leaf(std::string_view kind,
                       const std::wstring& leaf,
                       std::string* out) {
  const KindRel* spec = nullptr;
  for (const KindRel& row : kKindRels) {
    if (row.kind == kind) {
      spec = &row;
      break;
    }
  }
  if (!spec) {
    return false;
  }
  wchar_t parent[MAX_PATH] = {};
  wchar_t local[MAX_PATH] = {};
  if (swprintf_s(parent, MAX_PATH, spec->parent_fmt, leaf.c_str()) <= 0 ||
      swprintf_s(local, MAX_PATH, spec->local_fmt, leaf.c_str()) <= 0) {
    return false;
  }
  const wchar_t* rels[] = {parent, local};
  return resolve_canonical_rels(rels, 2, out);
}

bool resolve_harness_leaf(const std::string& leaf_utf8, std::string* out) {
  if (!out || leaf_utf8.empty()) {
    return false;
  }
  wchar_t exe[MAX_PATH] = {};
  if (GetModuleFileNameW(nullptr, exe, MAX_PATH) == 0) {
    return false;
  }
  std::wstring dir(exe);
  const size_t slash = dir.find_last_of(L"\\/");
  if (slash == std::wstring::npos) {
    return false;
  }
  dir.resize(slash);
  const std::wstring leaf_w = utf8_to_wide(leaf_utf8);
  if (leaf_w.empty()) {
    return false;
  }
  const std::wstring harness_rels[] = {
      dir + L"\\..\\..\\testing\\tools\\harness",
      dir + L"\\..\\..\\..\\testing\\tools\\harness",
  };
  for (const std::wstring& harness : harness_rels) {
    wchar_t abs_buf[MAX_PATH] = {};
    const DWORD got =
        GetFullPathNameW(harness.c_str(), MAX_PATH, abs_buf, nullptr);
    if (got == 0 || got >= MAX_PATH) {
      continue;
    }
    std::wstring found;
    if (find_named_under(abs_buf, leaf_w, &found, 0) && !found.empty() &&
        copy_utf8(found.c_str(), out)) {
      return true;
    }
  }
  return false;
}

bool resolve_data_leaf(const std::string& kind,
                       const std::string& leaf,
                       std::string* out) {
  if (!out || leaf.empty()) {
    return false;
  }
  if (kind == "harness") {
    return resolve_harness_leaf(leaf, out);
  }
  const std::wstring leaf_w = utf8_to_wide(leaf);
  if (leaf_w.empty()) {
    return false;
  }
  return resolve_kind_leaf(kind, leaf_w, out);
}

}  // namespace

bool resolve_first_existing_under_exe(const wchar_t* const* rels,
                                      size_t count,
                                      char* out_utf8,
                                      size_t out_cap) {
  if (!out_utf8 || out_cap < 2 || !rels || count == 0) {
    return false;
  }
  return walk_existing_under_exe(
      rels, count, false, [&](const wchar_t* wide) {
        return wide_to_utf8(wide, out_utf8, out_cap);
      });
}

void bind_paths(content::CapabilityHost* out) {
  bind_tagged_slots(
      out, base::tagged_tuple{
               base::tag_resolver<slot_resolve_data> =
                   [](const std::string& kind, const std::string& leaf,
                      std::string* out_path) {
                     return resolve_data_leaf(kind, leaf, out_path);
                   },
               base::tag_resolver<slot_capture_path> =
                   [](const std::string& leaf, std::string* out_path) {
                     if (!out_path || leaf.empty()) {
                       return false;
                     }
                     const std::wstring leaf_w = utf8_to_wide(leaf);
                     if (leaf_w.empty()) {
                       return false;
                     }
                     wchar_t path_w[MAX_PATH] = {};
                     if (!exe_capture_path(path_w, MAX_PATH, leaf_w.c_str())) {
                       return false;
                     }
                     return copy_utf8(path_w, out_path);
                   },
               base::tag_resolver<slot_sidecar_path> =
                   [](const std::string& rel, std::string* out_path) {
                     if (!out_path || rel.empty()) {
                       return false;
                     }
                     char path_a[MAX_PATH] = {};
                     if (!exe_sidecar_path_a(path_a, MAX_PATH, rel.c_str())) {
                       return false;
                     }
                     *out_path = path_a;
                     return true;
                   },
           });
}

}  // namespace detail
}  // namespace app
