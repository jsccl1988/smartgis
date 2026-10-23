// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

// Script verbs for the Web Mercator pyramid. Names are dynamic (register_fn),
// not new opcodes. eval_script_call dispatches them before the opcode table.

#ifndef CONTENT_BROWSER_GLANG_RUNTIME_HOST_TILE_VERBS_H_
#define CONTENT_BROWSER_GLANG_RUNTIME_HOST_TILE_VERBS_H_

#include "content/browser/glang.runtime/exec/py/script_call.h"

#include <string>

namespace content {
namespace tile {

// Registers tile_xyz, tile_bbox, tile_resolution, tile_url, tile_fetch.
void install_script_verbs();

// True when |call.name| is a tile verb (handled). False leaves outputs alone.
bool try_script(const content::code::ScriptCall& call, std::string* result,
                std::string* error);

}  // namespace tile
}  // namespace content

#endif  // CONTENT_BROWSER_GLANG_RUNTIME_HOST_TILE_VERBS_H_
