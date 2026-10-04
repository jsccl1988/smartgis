// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef APP_UI_DESIGNER_DOCUMENT_DOM_H_
#define APP_UI_DESIGNER_DOCUMENT_DOM_H_

#include <string>

#include "pugixml.hpp"

namespace app {

// Depth-first lookup of an element with attribute id == |id|.
pugi::xml_node find_by_id(pugi::xml_node root, const std::string& id);

// First non-<style> child of <ui> (or document element) — the editable root.
pugi::xml_node content_root(const pugi::xml_document* doc);

}  // namespace app

#endif  // APP_UI_DESIGNER_DOCUMENT_DOM_H_
