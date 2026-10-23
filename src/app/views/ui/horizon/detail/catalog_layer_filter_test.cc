// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/views/ui/horizon/detail/catalog_layer_filter.h"

#include <cstdio>
#include <string>
#include <vector>

namespace {

int g_fails = 0;

void expect(bool ok, const char* msg) {
  if (!ok) {
    std::fprintf(stderr, "FAIL: %s\n", msg);
    ++g_fails;
  }
}

content::LayerDesc leaf(std::string id, std::string name) {
  content::LayerDesc d;
  d.id = std::move(id);
  d.name = std::move(name);
  d.visible = true;
  return d;
}

}  // namespace

int main() {
  using app::detail::CatalogViewMode;
  using app::detail::filter_layer_descs_for_mode;

  {
    const std::vector<content::LayerDesc> in = {
        leaf("style.hillshade", "hillshade"),
        leaf("layer.area", "area"),
        leaf("layer.line", "line"),
    };
    const auto out = filter_layer_descs_for_mode(in, CatalogViewMode::kMap2d);
    expect(out.size() == 3u, "map2d keeps carto+land");
    expect(!out.empty() && out[0].id == "style.hillshade", "map2d hillshade");
    expect(out.size() >= 2 && out[1].id == "layer.area", "map2d land");
    expect(out.size() >= 3 && out[2].id == "layer.line", "map2d line");
  }

  {
    const std::vector<content::LayerDesc> in = {
        leaf("basemap.tiles", "Basemap"),
        leaf("style.hillshade", "hillshade"),
        leaf("layer.area", "area"),
        leaf("layer.line", "line"),
    };
    const auto out = filter_layer_descs_for_mode(in, CatalogViewMode::kScene3d);
    expect(out.size() == 1u, "scene3d land only");
    expect(!out.empty() && out[0].id == "layer.area", "scene3d area id");
  }

  {
    content::LayerDesc group;
    group.id = "group.china";
    group.name = "China";
    group.kind = content::LayerKind::kGroup;
    group.children.push_back(leaf("layer.area", "area"));
    group.children.push_back(leaf("layer.line", "line"));
    const auto out =
        filter_layer_descs_for_mode({group}, CatalogViewMode::kScene3d);
    expect(out.size() == 1u, "scene3d keeps group with land");
    expect(!out.empty() && out[0].id == "group.china", "scene3d group id");
    expect(!out.empty() && out[0].children.size() == 1u,
           "scene3d prunes non-land children");
    expect(!out.empty() && !out[0].children.empty() &&
               out[0].children[0].id == "layer.area",
           "scene3d land child");
  }

  if (g_fails) {
    std::fprintf(stderr, "%d catalog_layer_filter_test failure(s)\n", g_fails);
    return 1;
  }
  std::printf("catalog_layer_filter_test OK\n");
  return 0;
}
