// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/views/il.runtime/codegen/lower/document.h"

#include <string>

#include "app/views/il.runtime/codegen/lower/act.h"
#include "app/views/il.runtime/codegen/lower/args.h"
#include "app/views/il.runtime/codegen/lower/ops.h"
#include "app/views/il.runtime/ir/api.h"

namespace app {
namespace detail {
namespace {

// `var` wins when it is set; otherwise `path`, including a `$name` ident.
template <typename Pack>
std::string bound_path(const Pack& pack, VarMap* vars) {
  std::string path;
  if (vars && !pack["var"_t].empty()) {
    (void)lookup_var(vars, pack["var"_t], &path);
  }
  if (path.empty()) {
    path = resolve_ident(pack["path"_t], vars);
  }
  return path;
}

std::optional<Action> lower_open_document(const CallStmt& c, VarMap* vars) {
  return bind_action(
      c,
      make_named_tuple("path"_t = std::string(), named_only<"var"> = std::string(),
                       named_only<"fail_rc"> = 0),
      [vars](content::CapabilityHost& host, const auto& p) {
        return ir::note_fail(host, ir::open_document(host, bound_path(p, vars)),
                             p["fail_rc"_t]);
      });
}

std::optional<Action> lower_session(const CallStmt& c, VarMap* vars) {
  return lower_ops(c, vars, op(lower_open_document, "open_document"),
                   op(lower_host<ir::doc_clear>, "doc_clear"));
}

std::optional<Action> lower_export_bmp(const CallStmt& c, VarMap*) {
  return bind_action(
      c,
      make_named_tuple("leaf"_t = std::string(),
                       "frame"_t = std::string("document_extent")),
      [](content::CapabilityHost& host, const auto& p) {
        return ir::export_bmp(host, p["leaf"_t], p["frame"_t]);
      });
}

std::optional<Action> lower_apply_style_file(const CallStmt& c, VarMap* vars) {
  return bind_action(
      c,
      make_named_tuple("path"_t = std::string(),
                       named_only<"var"> = std::string()),
      [vars](content::CapabilityHost& host, const auto& p) {
        return ir::apply_style_file(host, bound_path(p, vars));
      });
}

std::optional<Action> lower_store(const CallStmt& c, VarMap* vars) {
  return lower_ops(c, vars, op(lower_host<ir::fit_extent>, "fit_extent"),
                   op(lower_export_bmp, "export_bmp"),
                   op(lower_apply_style_file, "apply_style_file"));
}

}  // namespace

std::optional<Action> try_lower_document_call(const CallStmt& c, VarMap* vars) {
  if (std::optional<Action> hit = lower_session(c, vars)) {
    return hit;
  }
  return lower_store(c, vars);
}

}  // namespace detail
}  // namespace app
