// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "content/embed/embed_sample.h"

#include <cstdio>

namespace {

int g_fails = 0;

void expect(bool ok, const char* msg) {
  if (!ok) {
    std::fprintf(stderr, "FAIL: %s\n", msg);
    ++g_fails;
  }
}

}  // namespace

int main() {
  expect(content::kExitEditConflict == 100, "reserved exit 100");
  expect(content::kExitEmbedOpenFailed == 101, "reserved exit 101");

  expect(!content::open_map_host_path(nullptr, "x.gpkg"), "null host rejected");
  content::EmbedMapHost empty{};
  expect(!content::open_map_host_path(&empty, ""), "empty path rejected");
  expect(!content::open_map_host_path(&empty, nullptr), "null path rejected");

  content::EmbedMapHost host{};
  const char* path = "testing/data/sample.gpkg";
  expect(content::open_map_host_path(&host, path), "open_map_host_path ok");
  expect(host.view_id != 0, "view id assigned");
  expect(host.path == path, "path recorded");
  expect(host.view_host.edits() != nullptr, "view host edits bound");
  expect(host.view_host.workspace() != nullptr, "view host workspace bound");
  expect(host.view_host.events() != nullptr, "view host events bound");

  content::EmbedMapHost host2{};
  expect(content::open_map_host_path(&host2, "map://demo"), "second open ok");
  expect(host2.view_id != 0, "second view id");

  if (g_fails) {
    std::fprintf(stderr, "%d failure(s)\n", g_fails);
    return content::kExitEmbedOpenFailed;
  }
  std::printf("content_embed_sample_test: ok\n");
  return 0;
}
