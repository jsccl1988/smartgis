// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "ui/views/kernel/shell/theme_service.h"

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <fstream>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <shlobj.h>

namespace ui {
namespace views {
namespace {

std::filesystem::path theme_persist_path() {
  wchar_t* folder = nullptr;
  if (FAILED(SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, nullptr, &folder)) ||
      !folder) {
    return {};
  }
  std::filesystem::path path(folder);
  CoTaskMemFree(folder);
  path /= L"SmartGIS";
  path /= L"ui_theme_id.txt";
  return path;
}

}  // namespace

Theme make_dark_theme() {
  Theme t;
  t.shell_bg = ui::gfx::color_rgb(30, 30, 30);
  t.panel_bg = ui::gfx::color_rgb(37, 37, 38);
  t.panel_header = ui::gfx::color_rgb(45, 45, 48);
  t.accent = ui::gfx::color_rgb(0, 122, 204);
  // Near-white body ink so AA glyph edges stay above score light_text (~170).
  t.text = ui::gfx::color_rgb(235, 235, 235);
  t.text_bright = ui::gfx::color_rgb(255, 255, 255);
  t.text_muted = ui::gfx::color_rgb(175, 175, 175);
  t.control_bg = ui::gfx::color_rgb(30, 30, 30);
  t.control_fill = ui::gfx::color_rgb(55, 55, 58);
  t.control_hover = ui::gfx::color_rgb(78, 78, 82);
  t.control_press = ui::gfx::color_rgb(48, 48, 52);
  t.control_disabled = ui::gfx::color_rgb(45, 45, 45);
  t.control_unchecked = ui::gfx::color_rgb(50, 50, 50);
  t.map_placeholder = ui::gfx::color_rgb(27, 58, 75);
  t.caption_bg = ui::gfx::color_rgb(37, 37, 38);
  t.caption_button_hover = ui::gfx::color_rgb(60, 60, 60);
  t.caption_close_hover = ui::gfx::color_rgb(196, 43, 28);
  return t;
}

Theme make_light_theme() {
  Theme t;
  t.shell_bg = ui::gfx::color_rgb(245, 245, 245);
  t.panel_bg = ui::gfx::color_rgb(255, 255, 255);
  t.panel_header = ui::gfx::color_rgb(232, 232, 232);
  t.accent = ui::gfx::color_rgb(0, 120, 212);
  t.text = ui::gfx::color_rgb(50, 50, 50);
  t.text_bright = ui::gfx::color_rgb(16, 16, 16);
  t.text_muted = ui::gfx::color_rgb(110, 110, 110);
  t.control_bg = ui::gfx::color_rgb(255, 255, 255);
  t.control_fill = ui::gfx::color_rgb(225, 225, 225);
  t.control_hover = ui::gfx::color_rgb(210, 210, 210);
  t.control_press = ui::gfx::color_rgb(195, 195, 195);
  t.control_disabled = ui::gfx::color_rgb(235, 235, 235);
  t.control_unchecked = ui::gfx::color_rgb(220, 220, 220);
  t.map_placeholder = ui::gfx::color_rgb(200, 220, 230);
  t.caption_bg = ui::gfx::color_rgb(243, 243, 243);
  t.caption_button_hover = ui::gfx::color_rgb(220, 220, 220);
  t.caption_close_hover = ui::gfx::color_rgb(232, 17, 35);
  return t;
}

ThemeService& ThemeService::get() {
  static ThemeService instance;
  return instance;
}

ThemeService::ThemeService() {
  ensure_builtin_packs();
  theme_ = make_dark_theme();
  current_id_ = "dark";
}

void ThemeService::ensure_builtin_packs() {
  if (builtins_ready_) {
    return;
  }
  builtins_ready_ = true;
  register_pack({"dark", "Dark", make_dark_theme()});
  register_pack({"light", "Light", make_light_theme()});
}

void ThemeService::register_pack(ThemePack pack) {
  if (pack.id.empty()) {
    return;
  }
  for (ThemePack& existing : packs_) {
    if (existing.id == pack.id) {
      existing = std::move(pack);
      return;
    }
  }
  packs_.push_back(std::move(pack));
}

bool ThemeService::set_theme(std::string_view id) {
  return set_theme(id, true);
}

bool ThemeService::set_theme(std::string_view id, bool persist_to_disk) {
  ensure_builtin_packs();
  const auto it =
      std::find_if(packs_.begin(), packs_.end(),
                   [id](const ThemePack& p) { return p.id == id; });
  if (it == packs_.end()) {
    return false;
  }
  const bool id_changed = current_id_ != it->id;
  current_id_ = it->id;
  theme_ = it->colors;
  if (persist_to_disk && id_changed) {
    persist();
  }
  if (id_changed) {
    for (ThemeObserver* obs : observers_) {
      if (obs) {
        obs->on_theme_changed();
      }
    }
  }
  return true;
}

void ThemeService::add_observer(ThemeObserver* observer) {
  if (!observer) {
    return;
  }
  if (std::find(observers_.begin(), observers_.end(), observer) ==
      observers_.end()) {
    observers_.push_back(observer);
  }
}

void ThemeService::remove_observer(ThemeObserver* observer) {
  observers_.erase(
      std::remove(observers_.begin(), observers_.end(), observer),
      observers_.end());
}

void ThemeService::load_persisted() {
  ensure_builtin_packs();
  const std::filesystem::path path = theme_persist_path();
  if (path.empty() || !std::filesystem::exists(path)) {
    return;
  }
  std::ifstream in(path);
  std::string id;
  if (in >> id) {
    set_theme(id);
  }
}

void ThemeService::persist() const {
  const std::filesystem::path path = theme_persist_path();
  if (path.empty() || current_id_.empty()) {
    return;
  }
  std::error_code ec;
  std::filesystem::create_directories(path.parent_path(), ec);
  std::ofstream out(path, std::ios::trunc);
  if (out) {
    out << current_id_;
  }
}

}  // namespace views
}  // namespace ui
