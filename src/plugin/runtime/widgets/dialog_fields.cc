// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "plugin/runtime/widgets/dialog_fields.h"

#include <cstdlib>
#include <cstring>

#include "base/process/switches.h"
#include "content/browser/glang.runtime/host/utils/exe_path.h"
#include "ui/views/markup/loader/markup_loader.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

namespace plugin {
namespace {

// Sample trees live at <gen_root>/../data/plugin (exe in out/Debug|Release).
// Resolve under the module dir — never against process CWD (IDE launches often
// leave CWD at the repo root, so bare "../data/plugin" points outside the tree).
std::string resolve_sample_dir_under_exe() {
  char base[MAX_PATH] = {};
  if (!content::detail::exe_dir_with_slash_a(base, MAX_PATH)) {
    return {};
  }
  static constexpr const char* kRels[] = {"../data/plugin", "data/plugin"};
  for (const char* rel : kRels) {
    char full[MAX_PATH] = {};
    if (strcpy_s(full, base) != 0 || strcat_s(full, rel) != 0) {
      continue;
    }
    if (GetFileAttributesA(full) == INVALID_FILE_ATTRIBUTES) {
      continue;
    }
    char canon[MAX_PATH] = {};
    if (GetFullPathNameA(full, MAX_PATH, canon, nullptr) != 0 && canon[0]) {
      return std::string(canon);
    }
    return std::string(full);
  }
  char fallback[MAX_PATH] = {};
  if (strcpy_s(fallback, base) != 0 ||
      strcat_s(fallback, "../data/plugin") != 0) {
    return {};
  }
  char canon[MAX_PATH] = {};
  if (GetFullPathNameA(fallback, MAX_PATH, canon, nullptr) != 0 && canon[0]) {
    return std::string(canon);
  }
  return std::string(fallback);
}

}  // namespace

double parse_field_double_c(const char* text, double fallback) {
  if (!text || !text[0]) {
    return fallback;
  }
  char* end = nullptr;
  const double v = std::strtod(text, &end);
  return end != text ? v : fallback;
}

std::string plugin_sample_dir() {
  const char* sample_dir = base::switch_cstr("plugin-sample-dir");
  if (sample_dir && *sample_dir) {
    return std::string(sample_dir);
  }
  std::string under_exe = resolve_sample_dir_under_exe();
  if (!under_exe.empty()) {
    return under_exe;
  }
  return "../data/plugin";
}

ui::views::MarkupRoot* load_dialog_markup(std::string_view path_or_name) {
  ui::views::MarkupRoot* loaded = ui::views::markup_root_acquire();
  if (!loaded) {
    return nullptr;
  }
  if (path_or_name.empty() ||
      !ui::views::load_markup_into(path_or_name, {}, loaded) ||
      !loaded->ok()) {
    ui::views::markup_root_release(loaded);
    return nullptr;
  }
  return loaded;
}

ui::views::View* take_dialog_markup_root(ui::views::MarkupRoot* root) {
  ui::views::View* body = ui::views::markup_root_take_root(root);
  ui::views::markup_root_release(root);
  return body;
}

}  // namespace plugin
