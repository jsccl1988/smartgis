// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/views/runtime/interact/policy/bind.h"

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

app::detail::Arg named_int(std::string name, int v) {
  using namespace base::tuple::literals;
  app::detail::Arg a;
  a["name"_t] = std::move(name);
  a["value"_t] = app::detail::make_int(v);
  return a;
}

app::detail::Arg pos_int(int v) {
  using namespace base::tuple::literals;
  app::detail::Arg a;
  a["value"_t] = app::detail::make_int(v);
  return a;
}

app::detail::Arg pos_ident(std::string v) {
  using namespace base::tuple::literals;
  app::detail::Arg a;
  a["value"_t] = app::detail::make_ident(std::move(v));
  return a;
}

void test_bind_named_and_positional() {
  using namespace app::detail;
  CallStmt c;
  c.args.push_back(pos_ident("map_client"));
  c.args.push_back(pos_int(11));
  c.args.push_back(named_int("y", 22));
  auto p = make_named_tuple("target"_t = std::string("shell_client"), "x"_t = 0,
                            "y"_t = 0, "button"_t = std::string("left"));
  bind_into(c, p);
  expect(p["target"_t] == "map_client", "target positional");
  expect(p["x"_t] == 11, "x positional");
  expect(p["y"_t] == 22, "y named");
  expect(p["button"_t] == "left", "button default");
}

void test_bind_named_only_skips_index() {
  using namespace app::detail;
  CallStmt c;
  c.args.push_back(pos_ident("shell_client"));
  c.args.push_back(named_int("count", 9));
  auto p = make_named_tuple("target"_t = std::string("map_client"),
                            named_only<"count"> = 4);
  bind_into(c, p);
  expect(p["target"_t] == "shell_client", "target still positional 0");
  expect(p["count"_t] == 9, "count named_only");
}

void test_value_variant() {
  using namespace app::detail;
  Value v = make_ident("leaf");
  expect(is_text(v), "ident is text");
  expect(!is_int(v), "ident not int");
  const std::string* s = as_text(v);
  expect(s && *s == "leaf", "as_text");
}

}  // namespace

int main() {
  test_bind_named_and_positional();
  test_bind_named_only_skips_index();
  test_value_variant();
  if (g_fails != 0) {
    std::fprintf(stderr, "interact_bind_test: %d fail(s)\n", g_fails);
    return 1;
  }
  std::printf("interact_bind_test: ok\n");
  return 0;
}
