// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "content/browser/glang.runtime/exec/session/plugin_fn.h"

#if defined(Py_PYTHON_H) || defined(Py_LIMITED_API)
#error "plugin_fn.h must not include Python.h"
#endif

#include "gtest/gtest.h"

#include <string>
#include <string_view>
#include <unordered_map>

#if defined(HAS_PYTHON)
#include "content/browser/capability/host.h"
#include "content/browser/glang.runtime/exec/eval/eval.h"
#include "content/browser/glang.runtime/exec/session/cpython.h"
#endif

namespace {

void expect_call_known(std::string_view name) {
  content::code::ScriptCall call;
  call.name = std::string(name);
  std::string error;
  const bool ran = content::py::call_registered(call, &error);
  EXPECT_FALSE(ran) << name;
  EXPECT_NE(error.find("script host not bound"), std::string::npos) << name << ": " << error;
  EXPECT_EQ(error.find("unknown verb"), std::string::npos) << name << ": " << error;
}

TEST(PluginFn, ResolvesMarkClickAndGisName) {
  // No script host and no interpreter. A known name passes the table and
  // stops at "script host not bound" instead of "unknown verb".
  content::code::clear_script_handler();
  content::code::ensure_ops_registered();

  for (int i = 0; i < static_cast<int>(content::code::Op::kCount); ++i) {
    const content::code::OpDesc* desc =
        content::code::find_op(static_cast<content::code::Op>(i));
    if (!desc || !desc->name || !desc->name[0] || !desc->eval) {
      continue;
    }
    const std::optional<content::py::RegisteredFn> found = content::py::find_fn(desc->name);
    ASSERT_TRUE(found.has_value()) << desc->name;
    EXPECT_EQ(found->eval, desc->eval) << desc->name;
    EXPECT_EQ(found->param_count, desc->param_count) << desc->name;
    if (!desc->aliases) {
      continue;
    }
    for (const char* const* alias = desc->aliases; *alias; ++alias) {
      if (!*alias || !(*alias)[0]) {
        continue;
      }
      const std::optional<content::py::RegisteredFn> aliased = content::py::find_fn(*alias);
      ASSERT_TRUE(aliased.has_value()) << *alias;
      EXPECT_EQ(aliased->eval, desc->eval) << *alias;
      expect_call_known(*alias);
    }
  }

  const std::optional<content::py::RegisteredFn> mark = content::py::find_fn("mark");
  const std::optional<content::py::RegisteredFn> click = content::py::find_fn("click");
  const std::optional<content::py::RegisteredFn> layers = content::py::find_fn("list_layers");
  const std::optional<content::py::RegisteredFn> buffer = content::py::find_fn("buffer");
  const std::optional<content::py::RegisteredFn> intersection =
      content::py::find_fn("intersection");
  const std::optional<content::py::RegisteredFn> open_map = content::py::find_fn("open_map");
  const std::optional<content::py::RegisteredFn> open_document =
      content::py::find_fn("open_document");
  ASSERT_TRUE(mark.has_value());
  ASSERT_TRUE(click.has_value());
  ASSERT_TRUE(layers.has_value());
  ASSERT_TRUE(buffer.has_value());
  ASSERT_TRUE(intersection.has_value());
  ASSERT_TRUE(open_map.has_value());
  ASSERT_TRUE(open_document.has_value());
  EXPECT_EQ(open_map->eval, open_document->eval);
  EXPECT_NE(mark->eval, nullptr);
  EXPECT_NE(click->eval, nullptr);
  EXPECT_NE(layers->eval, nullptr);
  EXPECT_NE(buffer->eval, nullptr);
  EXPECT_NE(intersection->eval, nullptr);
  EXPECT_NE(mark->eval, click->eval);
  EXPECT_FALSE(content::py::find_fn("not_a_registered_verb").has_value());

  expect_call_known("mark");
  expect_call_known("click");
  expect_call_known("list_layers");
  expect_call_known("open_map");

  content::code::ScriptCall missing;
  missing.name = "not_a_registered_verb";
  std::string error;
  EXPECT_FALSE(content::py::call_registered(missing, &error));
  EXPECT_NE(error.find("unknown verb"), std::string::npos) << error;
}

#if defined(HAS_PYTHON)

class RecordingHandler final : public content::code::ScriptHandler {
 public:
  explicit RecordingHandler(content::CapabilityHost& host) : host_(host) {}

  bool call(const content::code::ScriptCall& call,
            std::string* error,
            std::string* result) override {
    return content::code::eval_script_call(host_, call, &vars_, error, result);
  }

 private:
  content::CapabilityHost& host_;
  std::unordered_map<std::string, std::string> vars_;
};

TEST(PluginFn, UnknownVerbThrowsAfterPrefixMark) {
  ASSERT_TRUE(content::py::interpreter().init());
  content::CapabilityHost host;
  bool marked = false;
  std::string marked_token;
  host.horizon.mark = [&](const std::string& token) {
    marked = true;
    marked_token = token;
  };
  host.plugin.resolve_data = [](const std::string& kind, const std::string& leaf,
                                std::string* out) {
    if (!out) {
      return false;
    }
    *out = kind + ":" + leaf;
    return true;
  };
  bool opened = false;
  host.document.open_document = [&](const std::string& path) {
    opened = !path.empty();
    return opened;
  };

  RecordingHandler handler(host);
  content::code::set_script_handler(&handler);
  const char* source = R"PY(
import glang.plugin as g
g.mark("prefix-mark")
unknown = ""
try:
    g.not_a_real_verb_zz()
except RuntimeError as exc:
    unknown = str(exc)
path = g.resolve_data(kind="data", leaf="china_city.gpkg")
open_err = ""
try:
    g.open_map(path)
except RuntimeError as exc:
    open_err = str(exc)
click_err = ""
try:
    g.click("map_client", 1, 2)
except RuntimeError as exc:
    click_err = str(exc)
)PY";
  std::string error;
  const bool ran = content::py::interpreter().exec_source(source, &error);
  EXPECT_TRUE(ran) << error;
  EXPECT_TRUE(marked);
  EXPECT_EQ(marked_token, "prefix-mark");
  const std::string unknown = content::py::interpreter().eval("unknown");
  EXPECT_NE(unknown.find("unknown verb"), std::string::npos) << unknown;
  const std::string path = content::py::interpreter().eval("path");
  EXPECT_NE(path.find("data:china_city.gpkg"), std::string::npos) << path;
  const std::string open_err = content::py::interpreter().eval("open_err");
  EXPECT_EQ(open_err.find("unknown verb"), std::string::npos) << open_err;
  EXPECT_TRUE(opened);
  const std::string click_err = content::py::interpreter().eval("click_err");
  EXPECT_EQ(click_err.find("unknown verb"), std::string::npos) << click_err;
  content::code::clear_script_handler();
  content::py::interpreter().shutdown();
}

#endif

}  // namespace
