// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/ui_designer/document/edit.h"

#include <string>
#include <vector>

#include "pugixml.hpp"

#include "app/ui_designer/document/dom.h"

namespace app {

bool apply_markup_fragment(ui::views::MarkupDocument& document,
                           const std::string& selected_id,
                           const std::string& fragment,
                           bool replace,
                           std::string* error) {
  auto fail = [&](const char* msg) {
    if (error) {
      *error = msg;
    }
    return false;
  };

  pugi::xml_document frag_doc;
  const pugi::xml_parse_result pr = frag_doc.load_string(fragment.c_str());
  if (!pr) {
    return fail("generate failed: fragment XML parse");
  }
  pugi::xml_node frag_root = frag_doc.first_child();
  while (frag_root && frag_root.type() != pugi::node_element) {
    frag_root = frag_root.next_sibling();
  }
  if (!frag_root) {
    return fail("generate failed: empty fragment root");
  }

  pugi::xml_document* doc = document.raw_xml();
  if (!doc) {
    return fail("generate failed: no document");
  }
  pugi::xml_node ui = doc->child("ui");
  if (!ui) {
    ui = doc->append_child("ui");
    ui.append_attribute("name") = "generated";
  }

  auto append_nodes = [&](pugi::xml_node parent) {
    if (std::string(frag_root.name()) == "ui") {
      for (pugi::xml_node c : frag_root.children()) {
        if (c.type() != pugi::node_element) {
          continue;
        }
        if (std::string(c.name()) == "style") {
          continue;
        }
        parent.append_copy(c);
      }
    } else {
      parent.append_copy(frag_root);
    }
  };

  if (replace) {
    std::vector<pugi::xml_node> doomed;
    for (pugi::xml_node c : ui.children()) {
      if (c.type() != pugi::node_element) {
        continue;
      }
      if (std::string(c.name()) == "style") {
        continue;
      }
      doomed.push_back(c);
    }
    for (pugi::xml_node n : doomed) {
      ui.remove_child(n);
    }
    append_nodes(ui);
  } else {
    pugi::xml_node parent = find_by_id(content_root(doc), selected_id);
    if (!parent) {
      parent = content_root(doc);
    }
    if (!parent) {
      append_nodes(ui);
    } else {
      append_nodes(parent);
    }
  }
  return true;
}

bool reorder_xml_sibling(ui::views::MarkupDocument& document,
                         const std::string& selected_id,
                         int delta) {
  pugi::xml_node node =
      find_by_id(content_root(document.raw_xml()), selected_id);
  if (!node || !node.parent()) {
    return false;
  }
  pugi::xml_node parent = node.parent();
  if (delta < 0) {
    pugi::xml_node prev = node.previous_sibling();
    while (prev && prev.type() != pugi::node_element) {
      prev = prev.previous_sibling();
    }
    if (!prev) {
      return false;
    }
    parent.insert_move_before(node, prev);
  } else {
    pugi::xml_node next = node.next_sibling();
    while (next && next.type() != pugi::node_element) {
      next = next.next_sibling();
    }
    if (!next) {
      return false;
    }
    parent.insert_move_after(node, next);
  }
  return true;
}

DropReorderResult apply_drop_reorder_xml(ui::views::MarkupDocument& document,
                                         const std::string& moving,
                                         const std::string& before_id,
                                         bool append) {
  pugi::xml_node node =
      find_by_id(content_root(document.raw_xml()), moving);
  if (!node || !node.parent()) {
    return DropReorderResult::kMissing;
  }
  pugi::xml_node parent = node.parent();
  if (append) {
    pugi::xml_node last;
    for (pugi::xml_node sib = parent.last_child(); sib;
         sib = sib.previous_sibling()) {
      if (sib.type() == pugi::node_element && sib != node) {
        last = sib;
        break;
      }
    }
    if (!last) {
      return DropReorderResult::kNoop;
    }
    pugi::xml_node after = node.next_sibling();
    while (after && after.type() != pugi::node_element) {
      after = after.next_sibling();
    }
    if (!after) {
      return DropReorderResult::kNoop;
    }
    parent.insert_move_after(node, last);
  } else {
    if (before_id.empty()) {
      return DropReorderResult::kMissing;
    }
    pugi::xml_node before =
        find_by_id(content_root(document.raw_xml()), before_id);
    if (!before || before.parent() != parent) {
      return DropReorderResult::kNotSibling;
    }
    pugi::xml_node walk = node.next_sibling();
    while (walk && walk.type() != pugi::node_element) {
      walk = walk.next_sibling();
    }
    if (walk == before) {
      return DropReorderResult::kNoop;
    }
    parent.insert_move_before(node, before);
  }
  return DropReorderResult::kOk;
}

}  // namespace app
