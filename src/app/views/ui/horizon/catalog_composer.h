// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef APP_VIEWS_UI_HORIZON_CATALOG_COMPOSER_H_
#define APP_VIEWS_UI_HORIZON_CATALOG_COMPOSER_H_

namespace app {

class BrowserView;

// Catalog layer tree wire + sync from MapScene.
class CatalogComposer {
 public:
  explicit CatalogComposer(BrowserView* host);
  ~CatalogComposer() = default;

  CatalogComposer(const CatalogComposer&) = delete;
  CatalogComposer& operator=(const CatalogComposer&) = delete;

  void wire_catalog();
  void sync_catalog_from_scene();

 private:
  BrowserView* host_ = nullptr;
};

}  // namespace app

#endif  // APP_VIEWS_UI_HORIZON_CATALOG_COMPOSER_H_
