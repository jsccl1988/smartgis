// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "vista/assets/tileset/tileset.h"

#include <cstdio>
#include <cstring>
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

bool resolve_stub(const char* uri, vista::ModelAsset* out, size_t* byte_cost,
                  void* user) {
  (void)user;
  if (!uri || !out || !byte_cost) {
    return false;
  }
  // Simulate missing / broken content for "miss.glb".
  if (std::strcmp(uri, "miss.glb") == 0) {
    return false;
  }
  out->name = uri;
  out->meshes.clear();
  // Charge a fixed stub cost so the LRU budget is deterministic.
  *byte_cost = 400;
  return true;
}

}  // namespace

int main() {
  const char* json =
      "{\"root\":{\"boundingVolume\":{\"box\":[0,0,0,2,0,0,0,2,0,0,0,2]},"
      "\"geometricError\":10,\"content\":{\"uri\":\"a.glb\"},"
      "\"children\":["
      "{\"boundingVolume\":{\"box\":[4,0,0,1,0,0,0,1,0,0,0,1]},"
      "\"geometricError\":0,\"content\":{\"uri\":\"b.glb\"}},"
      "{\"boundingVolume\":{\"box\":[8,0,0,1,0,0,0,1,0,0,0,1]},"
      "\"geometricError\":0,\"content\":{\"uri\":\"c.glb\"}},"
      "{\"boundingVolume\":{\"box\":[12,0,0,1,0,0,0,1,0,0,0,1]},"
      "\"geometricError\":0,\"content\":{\"uri\":\"miss.glb\"}}"
      "]}}";

  vista::Tileset tileset;
  expect(vista::parse_tileset_json(json, std::strlen(json), tileset),
         "parse fixture");
  expect(tileset.root.children.size() == 3, "three children");

  vista::ViewState view;
  view.eye_x = 0;
  view.eye_y = 0;
  view.eye_z = 8;
  view.sse_denominator = 1;

  std::vector<const vista::Tile*> visible;
  vista::select_tiles_limited(tileset, view, 0, 2, visible);
  expect(visible.size() == 2, "limited to 2");

  vista::TilesetContentCache cache(700);  // fits one 400-byte entry + stub room
  expect(cache.max_bytes() == 700, "budget");

  vista::ModelAsset a;
  a.name = "a";
  expect(cache.put("a.glb", a, 400, true), "put a");
  expect(cache.entry_count() == 1 && cache.resident_bytes() == 400, "one entry");

  vista::ModelAsset b;
  b.name = "b";
  expect(cache.put("b.glb", b, 400, true), "put b evicts a");
  expect(cache.try_get("a.glb") == nullptr, "a evicted");
  expect(cache.try_get("b.glb") != nullptr, "b resident");
  expect(cache.resident_bytes() == 400, "still one slot");

  // Oversize single payload is rejected.
  vista::ModelAsset huge;
  expect(!cache.put("huge.glb", huge, 701, true), "reject oversize");

  visible.clear();
  vista::select_tiles(tileset, view, 0, visible);
  expect(visible.size() == 3, "REPLACE leaves three leaves");

  vista::ensure_tileset_content(visible, &cache, resolve_stub, nullptr);
  // Budget 700: last successful puts leave at most one 400-byte asset; failed
  // miss.glb costs 1. Order of ensure walks visible — LRU may keep last hits.
  expect(cache.resident_bytes() <= cache.max_bytes(), "under budget");
  expect(cache.contains("miss.glb"), "failed uri stubbed");
  const vista::TilesetContentEntry* miss = cache.try_get("miss.glb");
  expect(miss != nullptr && !miss->decode_ok, "miss marked failed");

  // Second ensure must not grow without new URIs (touch only).
  const size_t bytes_before = cache.resident_bytes();
  const size_t count_before = cache.entry_count();
  vista::ensure_tileset_content(visible, &cache, resolve_stub, nullptr);
  expect(cache.resident_bytes() == bytes_before, "stable bytes");
  expect(cache.entry_count() == count_before, "stable count");

  if (g_fails) {
    std::fprintf(stderr, "tileset_test: %d failed\n", g_fails);
    return 1;
  }
  std::fprintf(stdout, "tileset_test: ok\n");
  return 0;
}
