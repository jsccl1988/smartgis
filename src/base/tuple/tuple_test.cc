// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "base/tuple/tuple.h"

#include <cstdio>
#include <string>

namespace {

int g_fails = 0;

void expect(bool ok, const char* msg) {
  if (!ok) {
    std::fprintf(stderr, "FAIL: %s\n", msg);
    ++g_fails;
  }
}

void test_named_tuple() {
  using namespace base::tuple::literals;
  auto rec = base::make_named_tuple("price"_t = 42, "size"_t = 100);
  expect(rec["price"_t] == 42, "named price");
  expect(rec.get<"size">() == 100, "named size get");
  rec["size"_t] = 200;
  expect(rec["size"_t] == 200, "named size assign");
}

void test_named_only() {
  using namespace base::tuple::literals;
  auto rec = base::make_named_tuple("target"_t = std::string("map"),
                                    base::named_only<"count"> = 4);
  expect(rec["target"_t] == "map", "named_only keeps target");
  expect(rec["count"_t] == 4, "named_only count");
}

void test_tagged_tuple() {
  struct PriceTag {};
  struct SizeTag {};
  auto trade = base::tagged_tuple{base::tag_resolver<PriceTag> = 42,
                                  base::tag_resolver<SizeTag> = 100};
  expect(base::tag_resolver<PriceTag>(trade) == 42, "tagged price");
  expect(base::get_tag<SizeTag>(trade) == 100, "tagged size");
  base::tag_resolver<SizeTag>(trade) = 7;
  expect(base::get_tag<SizeTag>(trade) == 7, "tagged assign");
}

void test_for_each() {
  auto t = std::make_tuple(1, 2.0);
  int n = 0;
  base::tuple::for_each(t, [&](auto&&) { ++n; });
  expect(n == 2, "for_each arity");
}

}  // namespace

int main() {
  test_named_tuple();
  test_named_only();
  test_tagged_tuple();
  test_for_each();
  if (g_fails != 0) {
    std::fprintf(stderr, "tuple_test: %d fail(s)\n", g_fails);
    return 1;
  }
  std::printf("tuple_test: ok\n");
  return 0;
}
