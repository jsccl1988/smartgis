// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "content/browser/glang.runtime/exec/op/register.h"

#include "content/browser/glang.runtime/exec/op/host_verb.h"

#include <cstddef>
#include <string_view>

namespace content {
namespace code {

// Host fills this before the opcode table is frozen. Language slots stay empty
// until install() runs, which is a no-op when the host is not linked.
struct HostVerbTable {
  static constexpr int kMax = 24;
  const OpDesc* items[kMax]{};
  int count = 0;
  void (*install)() = nullptr;
  bool installed = false;
};

HostVerbTable& host_verb_table() {
  static HostVerbTable table;
  return table;
}

void install_host_verbs() {
  HostVerbTable& table = host_verb_table();
  if (table.installed || !table.install) {
    return;
  }
  table.installed = true;
  table.install();
}

namespace {

static_assert(static_cast<int>(Op::kCount) == 105, "opcode table size");

// One script name (canonical or alias) and the descriptor it selects.
struct NameRef {
  const char* name;
  const OpDesc* desc;
};

// Fixed table. Filled once from the function-local static; no later heap.
struct Registry {
  static constexpr int kMaxNames = 160;
  const OpDesc* by_op[static_cast<std::size_t>(Op::kCount)]{};
  NameRef names[kMaxNames]{};
  int name_count = 0;
};

void add_name(Registry* reg, const char* name, const OpDesc* desc) {
  if (!reg || !name || !name[0] || !desc) {
    return;
  }
  for (int i = 0; i < reg->name_count; ++i) {
    if (std::string_view(reg->names[i].name) == name) {
      LOGGING(LOG_ERROR, "code: duplicate op name %s", name);
      return;
    }
  }
  if (reg->name_count >= Registry::kMaxNames) {
    LOGGING(LOG_ERROR, "code: op name table full at %s", name);
    return;
  }
  reg->names[reg->name_count++] = NameRef{name, desc};
}

Registry build_registry() {
  Registry reg;
  install_host_verbs();
  fill_document_ops(reg.by_op);
  fill_view_ops(reg.by_op);
  fill_plugin_ops(reg.by_op);
  fill_horizon_ops(reg.by_op);
  fill_gesture_ops(reg.by_op);
  fill_buffer_op(reg.by_op);
  fill_gis_ops(reg.by_op);
  const HostVerbTable& host_verbs = host_verb_table();
  for (int i = 0; i < host_verbs.count; ++i) {
    const OpDesc* desc = host_verbs.items[i];
    if (!desc) {
      continue;
    }
    const auto index = static_cast<std::size_t>(desc->op);
    if (index >= static_cast<std::size_t>(Op::kCount)) {
      LOGGING(LOG_ERROR, "code: host verb %s has no slot",
              desc->name ? desc->name : "op");
      continue;
    }
    if (reg.by_op[index]) {
      LOGGING(LOG_ERROR, "code: host verb %s collides with a language op",
              desc->name ? desc->name : "op");
      continue;
    }
    reg.by_op[index] = desc;
  }
  for (std::size_t i = 0; i < static_cast<std::size_t>(Op::kCount); ++i) {
    const OpDesc* desc = reg.by_op[i];
    if (!desc) {
      if (is_host_verb_slot(static_cast<Op>(i))) {
        continue;
      }
      LOGGING(LOG_ERROR, "code: missing descriptor for op %u",
              static_cast<unsigned>(i));
      continue;
    }
    add_name(&reg, desc->name, desc);
    if (!desc->aliases) {
      continue;
    }
    for (const char* const* alias = desc->aliases; *alias; ++alias) {
      add_name(&reg, *alias, desc);
    }
  }
  return reg;
}

const Registry& registry() {
  static const Registry reg = build_registry();
  return reg;
}

}  // namespace

const OpDesc* find_op(std::string_view script_name) {
  const Registry& reg = registry();
  for (int i = 0; i < reg.name_count; ++i) {
    if (script_name == reg.names[i].name) {
      return reg.names[i].desc;
    }
  }
  return nullptr;
}

const OpDesc* find_op(Op op) {
  const Registry& reg = registry();
  const auto index = static_cast<std::size_t>(op);
  if (index >= static_cast<std::size_t>(Op::kCount)) {
    return nullptr;
  }
  return reg.by_op[index];
}

void ensure_ops_registered() {
  (void)registry();
}

void set_host_verb_installer(void (*install)()) {
  host_verb_table().install = install;
}

void register_host_verb(const OpDesc* desc) {
  if (!desc || !desc->name || !desc->name[0] || !desc->eval) {
    return;
  }
  HostVerbTable& table = host_verb_table();
  for (int i = 0; i < table.count; ++i) {
    if (table.items[i] && table.items[i]->op == desc->op) {
      table.items[i] = desc;
      return;
    }
  }
  if (table.count >= HostVerbTable::kMax) {
    LOGGING(LOG_ERROR, "code: host verb table full at %s", desc->name);
    return;
  }
  table.items[table.count++] = desc;
}

}  // namespace code
}  // namespace content
