// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/ui_designer/designer_dom.h"

#include <string_view>

namespace app {

pugi::xml_node find_by_id(pugi::xml_node root, const std::string& id) {
  if (!root || id.empty()) {
    return {};
  }
  if (root.attribute("id").as_string("") == id) {
    return root;
  }
  for (pugi::xml_node child : root.children()) {
    if (child.type() != pugi::node_element) {
      continue;
    }
    pugi::xml_node hit = find_by_id(child, id);
    if (hit) {
      return hit;
    }
  }
  return {};
}

pugi::xml_node content_root(const pugi::xml_document* doc) {
  if (!doc) {
    return {};
  }
  pugi::xml_node ui = doc->child("ui");
  if (!ui) {
    ui = doc->document_element();
  }
  if (!ui) {
    return {};
  }
  for (pugi::xml_node child : ui.children()) {
    if (child.type() != pugi::node_element) {
      continue;
    }
    if (std::string_view(child.name()) == "style") {
      continue;
    }
    return child;
  }
  return {};
}

}  // namespace app
