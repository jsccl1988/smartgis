// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef APP_VIEWS_UI_HORIZON_CATALOG_COMPOSER_H_
#define APP_VIEWS_UI_HORIZON_CATALOG_COMPOSER_H_

namespace ui {
namespace views {
class CatalogView;
}  // namespace views
}  // namespace ui

namespace app {

class BrowserView;

// Dual CatalogView (2D / 3D) wire + sync from GisScene, paired with Map / 3D
// tabs. catalog_ on BrowserView always points at the active instance.
class CatalogComposer {
 public:
  explicit CatalogComposer(BrowserView* host);
  ~CatalogComposer() = default;

  CatalogComposer(const CatalogComposer&) = delete;
  CatalogComposer& operator=(const CatalogComposer&) = delete;

  void wire_catalog();
  void sync_catalog_from_scene();

  // Map / 3D tab became |index| — mirror onto catalog_mode_tabs_ + catalog_.
  void on_map_tab_changed(int index);
  // Catalog 2D / 3D tab became |index| — mirror onto map_tabs_ via switch.
  void on_catalog_mode_changed(int index);

 private:
  // Cross-DLL CatalogView/LayerTree baton (EXE must not pass std::function).
  struct ModeBaton {
    CatalogComposer* self = nullptr;
    int mode_index = 0;
  };

  void wire_one(ui::views::CatalogView* catalog, ModeBaton* baton);
  void populate_one(ui::views::CatalogView* catalog, int mode_index);
  void apply_active_catalog(int mode_index);

  void on_catalog_command(const char* id);
  void on_layer_visible(int mode_index, const char* id, bool visible);
  void on_layer_selected(int mode_index, const char* id);

  static void command_trampoline(void* baton, const char* id);
  static void visible_trampoline(void* baton, const char* id, bool visible);
  static void selection_trampoline(void* baton, const char* id);

  BrowserView* host_ = nullptr;
  bool syncing_mode_ = false;
  ModeBaton wire_baton_2d_{};
  ModeBaton wire_baton_3d_{};
};

}  // namespace app

#endif  // APP_VIEWS_UI_HORIZON_CATALOG_COMPOSER_H_
