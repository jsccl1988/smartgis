// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef UI_VIEWS_KERNEL_PAINT_PAINTER_REGISTRY_H_
#define UI_VIEWS_KERNEL_PAINT_PAINTER_REGISTRY_H_

#include "ui/ui_export.h"

#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "ui/views/kernel/paint/painter.h"

namespace ui {
namespace views {

// Process-wide map of paint_role → Painter. Builtins and plugins share this.
class UI_EXPORT PainterRegistry {
 public:
  static PainterRegistry& get();

  PainterRegistry(const PainterRegistry&) = delete;
  PainterRegistry& operator=(const PainterRegistry&) = delete;

  // Replaces any painter for |role|. Clears plugin ownership for that role.
  void register_painter(std::string_view role, std::unique_ptr<Painter> painter);

  // Like register_painter, but tagged so withdraw_plugin can restore the
  // previous entry (builtin or empty).
  void register_painter_for_plugin(std::string_view plugin_id,
                                   std::string_view role,
                                   std::unique_ptr<Painter> painter);

  Painter* find(std::string_view role) const;

  // Removes painters registered under |plugin_id|, restoring prior entries.
  void withdraw_plugin(std::string_view plugin_id);

 private:
  PainterRegistry() = default;

  // Move-only painters; Entry lives behind unique_ptr so map nodes only move
  // pointers (MSVC tree/list assign paths require that for move-only values).
  struct Entry {
    std::unique_ptr<Painter> painter;
    std::string plugin_id;  // empty = builtin / unowned
    std::unique_ptr<Painter> previous;
    std::string previous_plugin_id;
  };

  std::map<std::string, std::unique_ptr<Entry>> entries_;
};

}  // namespace views
}  // namespace ui

#endif  // UI_VIEWS_KERNEL_PAINT_PAINTER_REGISTRY_H_
