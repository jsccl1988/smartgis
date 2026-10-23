// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

// Opcode row installer. A lane declares one trait type per verb and hands the
// tuple to install_ops. The interpreter never switches on script names.

#ifndef CONTENT_BROWSER_GLANG_RUNTIME_OP_INSTALL_H_
#define CONTENT_BROWSER_GLANG_RUNTIME_OP_INSTALL_H_

#include "content/browser/glang.runtime/language/code/op.h"
#include "base/tuple/algorithm.h"

#include <cstddef>
#include <tuple>
#include <type_traits>
#include <utility>

namespace content {
namespace code {
namespace detail {

// Script name stored as a structural type so it can be a template argument.
template <std::size_t N>
struct OpName {
  char text[N];
  constexpr OpName(const char (&literal)[N]) : text{} {
    for (std::size_t i = 0; i < N; ++i) {
      text[i] = literal[i];
    }
  }
};

template <typename Fn>
constexpr bool op_produces() {
  if constexpr (requires { Fn::produces; }) {
    return Fn::produces;
  } else {
    return false;
  }
}

template <typename Fn>
constexpr const char* const* op_aliases() {
  if constexpr (requires { Fn::aliases; }) {
    return Fn::aliases;
  } else {
    return nullptr;
  }
}

// One opcode row: id, script name, parameter list, aliases, and eval.
template <typename Fn>
const OpDesc* registered_op() {
  static const OpDesc desc{
      Fn::id,        Fn::name,   op_aliases<Fn>(), Fn::params,
      Fn::arity,     op_produces<Fn>(), &Fn::apply};
  return &desc;
}

template <typename Tuple>
void install_ops(const OpDesc** by_op) {
  if (!by_op) {
    return;
  }
  base::tuple::for_each(Tuple{}, [&](auto sample) {
    using Fn = std::decay_t<decltype(sample)>;
    by_op[static_cast<std::size_t>(Fn::id)] = registered_op<Fn>();
    return 0;
  });
}

// Trait row around an existing eval function. Name is a string literal.
#define CONTENT_LANE_OP(Struct, Id, Name, Aliases, Params, Arity, Produces, Fn) \
  struct Struct {                                                               \
    static constexpr Op id = Id;                                                \
    static constexpr const char* name = Name;                                   \
    static constexpr const char* const* aliases = Aliases;                      \
    static constexpr const Param* params = Params;                              \
    static constexpr int arity = Arity;                                         \
    static constexpr bool produces = Produces;                                  \
    static bool apply(content::CapabilityHost& host,                            \
                      const Module& module,                                     \
                      const Inst& inst,                                         \
                      SlotTable& slots,                                         \
                      std::string* error) {                                     \
      return Fn(host, module, inst, slots, error);                              \
    }                                                                           \
  }

}  // namespace detail
}  // namespace code
}  // namespace content

#endif  // CONTENT_BROWSER_GLANG_RUNTIME_OP_INSTALL_H_
