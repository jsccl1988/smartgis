// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "algorithm/stat/evaluate.h"
#include "algorithm/stat/value_set.h"
#include "algorithm/stat/value_traits.h"

#include <cmath>
#include <cstdio>
#include <span>
#include <type_traits>
#include <vector>

namespace {

int g_fails = 0;

void expect(bool ok, const char* msg) {
  if (!ok) {
    std::fprintf(stderr, "FAIL: %s\n", msg);
    ++g_fails;
  }
}

bool almost_eq(double a, double b) {
  return std::fabs(a - b) < 1e-9;
}

void double_each(std::span<double> values) {
  for (double& v : values) {
    v *= 2.0;
  }
}

}  // namespace

int main() {
  static_assert(std::is_same_v<stat::value_traits<std::vector<double>>::coordinate_type,
                               double>);

  {
    stat::ValueSet vs;
    const double a[] = {8.0, 16.0};
    const double b[] = {1.0, 2.0};
    expect(vs.bind("A", a) == SMT_ERR_NONE, "bind A");
    expect(vs.bind("B", b) == SMT_ERR_NONE, "bind B");
    expect(stat::evaluate("[C]=([A]/8+[B])*9", vs) == SMT_ERR_NONE,
           "assign arithmetic");
    expect(vs.has("C"), "C created");
    auto c = vs.get("C");
    expect(c.size() == 2 && almost_eq(c[0], 18.0) && almost_eq(c[1], 36.0),
           "[C]=([A]/8+[B])*9");
  }

  {
    stat::ValueSet vs;
    const double a[] = {100.0, 1000.0};
    expect(vs.bind("A", a) == SMT_ERR_NONE, "bind A log");
    expect(stat::evaluate("[B]=[A]{log}10", vs) == SMT_ERR_NONE, "{log}");
    auto b = vs.get("B");
    expect(b.size() == 2 && almost_eq(b[0], 2.0) && almost_eq(b[1], 3.0),
           "[A]{log}10");
  }

  {
    stat::ValueSet vs;
    const double a[] = {2.718281828459045};
    expect(vs.bind("A", a) == SMT_ERR_NONE, "bind A ln");
    expect(stat::evaluate("[B]=[A]{ln}", vs) == SMT_ERR_NONE, "{ln}");
    auto b = vs.get("B");
    expect(b.size() == 1 && almost_eq(b[0], 1.0), "[A]{ln}");
  }

  {
    stat::ValueSet vs;
    const double a[] = {1.0, 4.0};
    expect(vs.bind("A", a) == SMT_ERR_NONE, "bind A plus");
    expect(stat::evaluate("[B]=[A]+2", vs) == SMT_ERR_NONE, "broadcast add");
    auto b = vs.get("B");
    expect(b.size() == 2 && almost_eq(b[0], 3.0) && almost_eq(b[1], 6.0), "[A]+2");
  }

  {
    stat::ValueSet vs;
    const double a[] = {0.0};
    expect(vs.bind("A", a) == SMT_ERR_NONE, "bind A sin");
    expect(stat::evaluate("[B]=sin([A])", vs) == SMT_ERR_NONE, "sin()");
    expect(almost_eq(vs.get("B")[0], 0.0), "sin(0)");
  }

  {
    stat::ValueSet vs;
    const double a[] = {3.0, 1.0};
    expect(vs.bind("A", a) == SMT_ERR_NONE, "bind A pow");
    expect(stat::evaluate("[B]=[A]^2", vs) == SMT_ERR_NONE, "pow");
    auto b = vs.get("B");
    expect(b.size() == 2 && almost_eq(b[0], 9.0) && almost_eq(b[1], 1.0), "[A]^2");
  }

  {
    stat::ValueSet vs;
    expect(stat::evaluate("[C]=[A]+1", vs) == SMT_ERR_FUNC_INNER,
           "missing field");
    expect(stat::evaluate("[[[", vs) == SMT_ERR_INVALID_PARAM, "bad syntax");
  }

  {
    expect(stat::register_function("double_each", double_each) == SMT_ERR_NONE,
           "register");
    stat::ValueSet vs;
    const double a[] = {1.5, 2.5};
    expect(vs.bind("A", a) == SMT_ERR_NONE, "bind A fn");
    expect(stat::evaluate("[B]=double_each([A])", vs) == SMT_ERR_NONE,
           "call registered");
    auto b = vs.get("B");
    expect(b.size() == 2 && almost_eq(b[0], 3.0) && almost_eq(b[1], 5.0),
           "double_each");
  }

  if (g_fails != 0) {
    std::fprintf(stderr, "%d failure(s)\n", g_fails);
    return 1;
  }
  return 0;
}
