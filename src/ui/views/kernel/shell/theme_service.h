// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef UI_VIEWS_KERNEL_SHELL_THEME_SERVICE_H_
#define UI_VIEWS_KERNEL_SHELL_THEME_SERVICE_H_

#include "ui/ui_export.h"

#include <string>
#include <string_view>
#include <vector>

#include "ui/views/kernel/shell/theme.h"

namespace ui {
namespace views {

// Named theme pack: id is stable wire/persist key; label is UI text.
struct ThemePack {
  std::string id;
  std::string label;
  Theme colors;
};

// Observer notified after the active theme pack changes.
class UI_EXPORT ThemeObserver {
 public:
  virtual ~ThemeObserver() = default;
  virtual void on_theme_changed() = 0;
};

// Process-wide theme pack registry. Extensible: register more packs later.
class UI_EXPORT ThemeService {
 public:
  static ThemeService& get();

  void ensure_builtin_packs();
  void register_pack(ThemePack pack);
  const std::vector<ThemePack>& packs() const { return packs_; }

  bool set_theme(std::string_view id);
  std::string_view current_id() const { return current_id_; }
  const Theme& theme() const { return theme_; }

  void add_observer(ThemeObserver* observer);
  void remove_observer(ThemeObserver* observer);

  void load_persisted();
  void persist() const;

 private:
  ThemeService();

  std::vector<ThemePack> packs_;
  std::string current_id_;
  Theme theme_;
  std::vector<ThemeObserver*> observers_;
  bool builtins_ready_ = false;
};

Theme make_dark_theme();
Theme make_light_theme();

}  // namespace views
}  // namespace ui

#endif  // UI_VIEWS_KERNEL_SHELL_THEME_SERVICE_H_
