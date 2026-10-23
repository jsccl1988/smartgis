// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "content/browser/glang.runtime/host/document/document.h"
#include "content/browser/glang.runtime/host/bind/shell.h"

#include <cstdio>
#include <fstream>
#include <memory>
#include <string>

#include "content/browser/glang.runtime/host/view/probe.h"
#include "content/browser/document/gis_scene.h"
#include "content/browser/present/map2d/map2d_presenter.h"
#include "content/browser/session/browser_session.h"
#include "content/public/types.h"
#include "content/public/tool_session.h"
#include "gis/style/document/style_document.h"
#include "plugin/runtime/host/capability/shell.h"

namespace content {
namespace detail {
namespace {

bool open_path_on(content::GisScene* doc, const std::string& path_utf8) {
  return doc && !path_utf8.empty() && doc->open_path(path_utf8);
}

}  // namespace

bool dispatch_edit_input(content::Shell& browser, const content::InputEvent& event) {
  content::ToolSession* host = active_map_host(browser);
  return host && host->dispatch_input(event);
}

bool apply_style_file(content::Shell& browser, const std::string& path_utf8) {
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

bool open_document(content::Shell& browser, const std::string& path_utf8) {
  if (!open_path_on(browser.document(), path_utf8)) {
    return false;
  }
  // ui.shell / --ui-showcase: china opens after skip-ambox empty start. Drop
  // the pre-china FlyCube latch so the next present rebuilds carto (otherwise
  // StaticReuse leaves a hollow black map hole while Layers still check).
  if (browser.note_product_map2d_surface_after_show()) {
    browser.invalidate_native_map();
  }
  return true;
}

bool open_document(plugin::HarnessShell& host, const std::string& path_utf8) {
  return open_path_on(host.document(), path_utf8);
}

bool clear_map_document(content::Shell& browser) {
  if (content::GisScene* doc = browser.document()) {
    doc->clear();
    doc->clear_style_document();
  }
  return true;
}

bool fit_map_document(content::Shell& browser) {
  browser.fit_map_extent();
  // Same latch drop after late china fit (open_map → fit_extent in ui.shell).
  if (browser.note_product_map2d_surface_after_show()) {
    browser.invalidate_native_map();
  }
  return true;
}

int feature_count(content::Shell& browser) {
  content::GisScene* doc = browser.document();
  if (!doc) {
    return -1;
  }
  return static_cast<int>(doc->feature_count());
}

bool invalidate_map2d_frame(content::Shell& browser) {
  // Explicit IL invalidate must drop the published MapIR. The soft path
  // no-ops when the content hash already matches, which keeps a hollow
  // frame after fit_extent (zoom defer) so the viewport never shows the
  // new plugin layer.
  if (content::Map2dPresenter* map2d = browser.map2d()) {
    map2d->force_invalidate_frame_cache();
  }
  return true;
}

bool create_layer(content::Shell& browser,
                  const std::string& name,
                  const std::string& geometry) {
  return browser.session()->create_layer(name, geometry);
}

bool remove_layer(content::Shell& browser, const std::string& id) {
  return browser.session()->remove_layer(id);
}

bool set_layer_visible(content::Shell& browser,
                       const std::string& id,
                       bool visible) {
  return browser.session()->set_layer_visible(id, visible);
}

bool select_layer(content::Shell& browser, const std::string& id) {
  return browser.session()->select_layer(id);
}

bool write_document(content::Shell& browser, const std::string& path_utf8) {
  return browser.session()->write_document(path_utf8);
}

bool clear_selection(content::Shell& browser) {
  browser.session()->clear_selection();
  return true;
}

int layer_count(content::Shell& browser) {
  return static_cast<int>(browser.session()->document_layer_count());
}

std::string layers_json(content::Shell& browser) {
  return content::layers_to_catalog_json(
      browser.session()->document_layer_descs());
}

std::string extent_json(content::Shell& browser) {
  double min_x = 0;
  double min_y = 0;
  double max_x = 0;
  double max_y = 0;
  if (!browser.session()->document_compute_extent(&min_x, &min_y, &max_x,
                                                 &max_y)) {
    return "null";
  }
  char buf[192];
  std::snprintf(buf, sizeof(buf),
                "{\"min_x\":%.6f,\"min_y\":%.6f,\"max_x\":%.6f,\"max_y\":%.6f}",
                min_x, min_y, max_x, max_y);
  return std::string(buf);
}

bool has_style_document(content::Shell& browser) {
  return browser.session()->document_has_style();
}

void clear_style(content::Shell& browser) {
  browser.session()->clear_style_document();
}

}  // namespace detail
}  // namespace content
