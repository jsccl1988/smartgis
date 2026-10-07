// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

// Second-product compile proof. This TU includes only content/public.
#include "content/public/content_client.h"
#include "content/public/map_bootstrap.h"
#include "content/public/map_contents.h"

#include <cstdio>

namespace {

int g_fails = 0;

void expect(bool ok, const char* msg) {
  if (!ok) {
    std::fprintf(stderr, "FAIL: %s\n", msg);
    ++g_fails;
  }
}

// Minimal embedder: browser-process hook only.
class EmbedClient : public content::ContentClient {
 public:
  int browser_calls = 0;

  int browser_main(const content::ContentMainParams&) override {
    ++browser_calls;
    return 0;
  }
};

}  // namespace

int main() {
  EmbedClient client;
  wchar_t exe[] = L"content_embed_test.exe";
  wchar_t* argv[] = {exe};
  content::ContentMainParams params;
  params.argc = 1;
  params.argv = argv;
  expect(content::content_main(params, client) == 0, "browser_main return");
  expect(client.browser_calls == 1, "ContentClient::browser_main once");

  expect(!content::sample_map_relative_paths().empty(),
         "map_bootstrap public export");

  content::MapContents* contents = content::MapContents::Create();
  expect(contents != nullptr, "MapContents::Create");
  if (contents != nullptr) {
    expect(!contents->IsOopRender(), "Create does not launch a child");
    delete contents;
  }

  if (g_fails != 0) {
    std::fprintf(stderr, "content_embed_test: %d failed\n", g_fails);
    return 1;
  }
  std::printf("content_embed_test: ok\n");
  return 0;
}
