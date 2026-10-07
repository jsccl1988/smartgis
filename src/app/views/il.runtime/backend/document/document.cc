// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/views/il.runtime/backend/document/document.h"

#include <fstream>
#include <memory>
#include <string>

#include "app/views/browser/browser.h"
#include "app/views/il.runtime/backend/view/probe.h"
#include "content/browser/document/gis_scene.h"
#include "content/browser/present/map2d/map2d_presenter.h"
#include "content/public/types.h"
#include "content/public/tool_session.h"
#include "gis/style/document/style_document.h"
#include "plugin/runtime/host/capability/shell.h"

namespace app {
namespace detail {
namespace {

bool open_path_on(content::GisScene* doc, const std::string& path_utf8) {
  return doc && !path_utf8.empty() && doc->open_path(path_utf8);
}

}  // namespace

bool dispatch_edit_input(Browser& browser, const content::InputEvent& event) {
  content::ToolSession* host = active_map_host(browser);
  return host && host->dispatch_input(event);
}

bool apply_style_file(Browser& browser, const std::string& path_utf8) {
  content::GisScene* doc = browser.document();
  if (!doc || path_utf8.empty()) {
    return false;
  }
  std::ifstream in(path_utf8, std::ios::binary);
  if (!in) {
    return false;
  }
  std::string json((std::istreambuf_iterator<char>(in)),
                   std::istreambuf_iterator<char>());
  if (json.empty()) {
    return false;
  }
  auto style = std::make_shared<gis::style::StyleDocument>();
  if (!gis::style::parse_style_document(json, style.get())) {
    return false;
  }
  doc->set_style_document(std::move(style));
  return doc->style_document() != nullptr;
}

bool open_document(Browser& browser, const std::string& path_utf8) {
  return open_path_on(browser.document(), path_utf8);
}

bool open_document(plugin::HarnessShell& host, const std::string& path_utf8) {
  return open_path_on(host.document(), path_utf8);
}

bool clear_map_document(Browser& browser) {
  if (content::GisScene* doc = browser.document()) {
    doc->clear();
    doc->clear_style_document();
  }
  return true;
}

bool fit_map_document(Browser& browser) {
  browser.fit_map_extent();
  return true;
}

bool invalidate_map2d_frame(Browser& browser) {
  if (content::Map2dPresenter* map2d = browser.map2d()) {
    map2d->invalidate_frame_cache();
  }
  return true;
}

}  // namespace detail
}  // namespace app
