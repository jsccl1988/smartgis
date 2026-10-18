// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "content/browser/glang.runtime/exec/session/script_load.h"

#include "content/browser/glang.runtime/host/utils/charset.h"
#include "content/browser/glang.runtime/host/utils/exe_path.h"
#include "content/browser/glang.runtime/host/utils/find_named.h"

#include <string>
#include <string_view>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include "base/process/switches.h"

namespace content {
namespace detail {
namespace {

constexpr wchar_t kPySuffix[] = L".py";

// Relative to the exe directory. Two-up is out/<config>; three-up covers a
// deeper staging tree. First existing harness root wins its layouts, then
// the next root is tried.
constexpr const wchar_t* kHarnessFromExe[] = {
    L"..\\..\\testing\\tools\\harness",
    L"..\\..\\..\\testing\\tools\\harness",
};

// One path segment under a harness root. Literals are fixed directory names;
// the other tokens are filled from the suite id.
enum class SegmentKind { kFamily, kSuiteId, kLeaf, kLiteral };

struct Segment {
  SegmentKind kind;
  const wchar_t* literal;
};

struct Layout {
  Segment segments[3];
  int count;
};

// Colocated suite dir first, then the historical family buckets. The last
// row is the ui.interact gesture body, which lives in ui/ui.interact/ rather
// than ui/ui.interact.host/.
constexpr Layout kHarnessLayouts[] = {
    {{{SegmentKind::kFamily, nullptr},
      {SegmentKind::kSuiteId, nullptr},
      {SegmentKind::kLeaf, nullptr}},
     3},
    {{{SegmentKind::kLiteral, L"plugin"},
      {SegmentKind::kSuiteId, nullptr},
      {SegmentKind::kLeaf, nullptr}},
     3},
    {{{SegmentKind::kLiteral, L"browser"},
      {SegmentKind::kSuiteId, nullptr},
      {SegmentKind::kLeaf, nullptr}},
     3},
    {{{SegmentKind::kLiteral, L"ui"},
      {SegmentKind::kSuiteId, nullptr},
      {SegmentKind::kLeaf, nullptr}},
     3},
    {{{SegmentKind::kLiteral, L"ui"},
      {SegmentKind::kLiteral, L"ui.interact"},
      {SegmentKind::kLeaf, nullptr}},
     3},
};

struct SuiteNames {
  std::wstring family;
  std::wstring suite_id;
  std::wstring leaf;
};

std::wstring wide_utf8(std::string_view text) {
  return utf8_to_wide(std::string(text).c_str());
}

SuiteNames names_for(std::string_view suite_id) {
  SuiteNames names;
  const size_t dot = suite_id.find('.');
  names.family = dot == std::string_view::npos ? std::wstring(L"browser")
                                                : wide_utf8(suite_id.substr(0, dot));
  names.suite_id = wide_utf8(suite_id);
  names.leaf = names.suite_id + kPySuffix;
  return names;
}

bool accept_existing(const std::wstring& path, std::wstring* out) {
  if (!wide_path_exists(path)) {
    return false;
  }
  *out = path;
  return true;
}

std::wstring render_layout(const Layout& layout,
                           const std::wstring& root,
                           const SuiteNames& names) {
  std::wstring path = root;
  for (int i = 0; i < layout.count; ++i) {
    path.push_back(L'\\');
    switch (layout.segments[i].kind) {
      case SegmentKind::kFamily:
        path += names.family;
        break;
      case SegmentKind::kSuiteId:
        path += names.suite_id;
        break;
      case SegmentKind::kLeaf:
        path += names.leaf;
        break;
      case SegmentKind::kLiteral:
        path += layout.segments[i].literal;
        break;
    }
  }
  return path;
}

bool absolute_path(const std::wstring& path, std::wstring* out) {
  wchar_t buf[MAX_PATH] = {};
  const DWORD n = GetFullPathNameW(path.c_str(), MAX_PATH, buf, nullptr);
  if (n == 0 || n >= MAX_PATH) {
    return false;
  }
  *out = buf;
  return true;
}

bool exe_directory(std::wstring* out) {
  wchar_t buf[MAX_PATH] = {};
  if (!exe_dir_with_slash(buf, MAX_PATH)) {
    return false;
  }
  *out = buf;
  if (!out->empty() && (out->back() == L'\\' || out->back() == L'/')) {
    out->pop_back();
  }
  return !out->empty();
}

bool probe_sidecar(const std::wstring& leaf, std::wstring* out) {
  wchar_t sidecar[MAX_PATH] = {};
  if (!exe_sidecar_path(sidecar, MAX_PATH, leaf.c_str())) {
    return false;
  }
  return accept_existing(sidecar, out);
}

bool probe_layouts(const std::wstring& harness_root,
                   const SuiteNames& names,
                   std::wstring* out) {
  for (const Layout& layout : kHarnessLayouts) {
    if (accept_existing(render_layout(layout, harness_root, names), out)) {
      return true;
    }
  }
  return find_named_under(harness_root, names.leaf, out, 0);
}

bool probe_harness_roots(const std::wstring& exe_dir,
                         const SuiteNames& names,
                         std::wstring* out) {
  for (const wchar_t* rel : kHarnessFromExe) {
    std::wstring abs;
    if (!absolute_path(exe_dir + L'\\' + rel, &abs) || !wide_path_exists(abs)) {
      continue;
    }
    if (probe_layouts(abs, names, out)) {
      return true;
    }
  }
  return false;
}

}  // namespace

bool wide_path_exists(const std::wstring& path) {
  return !path.empty() &&
         GetFileAttributesW(path.c_str()) != INVALID_FILE_ATTRIBUTES;
}

bool locate_suite(const char* suite_id,
                  const char* switch_name,
                  std::wstring* out) {
  if (!out || !suite_id || !suite_id[0]) {
    return false;
  }
  if (switch_name) {
    if (const char* override_path = base::switch_cstr(switch_name)) {
      if (accept_existing(utf8_to_wide(override_path), out)) {
        return true;
      }
    }
  }

  const SuiteNames names = names_for(suite_id);
  if (probe_sidecar(names.leaf, out)) {
    return true;
  }
  std::wstring exe_dir;
  if (!exe_directory(&exe_dir)) {
    return false;
  }
  if (probe_harness_roots(exe_dir, names, out)) {
    return true;
  }
  return accept_existing(exe_dir + L'\\' + names.leaf, out);
}

}  // namespace detail
}  // namespace content
