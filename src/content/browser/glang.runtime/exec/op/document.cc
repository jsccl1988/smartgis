// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "content/browser/glang.runtime/exec/op/register.h"

#include "content/browser/glang.runtime/language/ir/document.h"

namespace content {
namespace code {
namespace {

bool eval_open_document(content::CapabilityHost& host,
                        const Module& module,
                        const Inst& inst,
                        SlotTable& slots,
                        std::string* error) {
  const std::string path = bound_path(module, inst, slots, 0, 1);
  const int fail_rc = arg_i32_param(module, inst, slots, 2);
  const bool opened = ir::open_document(host, path);
  return ok_step(inst, ir::note_fail(host, opened, fail_rc), error);
}

bool eval_doc_clear(content::CapabilityHost& host,
                    const Module&,
                    const Inst& inst,
                    SlotTable&,
                    std::string* error) {
  return ok_step(inst, ir::doc_clear(host), error);
}

bool eval_fit_extent(content::CapabilityHost& host,
                     const Module&,
                     const Inst& inst,
                     SlotTable&,
                     std::string* error) {
  return ok_step(inst, ir::fit_extent(host), error);
}

bool eval_export_bmp(content::CapabilityHost& host,
                     const Module& module,
                     const Inst& inst,
                     SlotTable& slots,
                     std::string* error) {
  return ok_step(inst,
                 ir::export_bmp(host, arg_str_param(module, inst, slots, 0),
                                arg_str_param(module, inst, slots, 1)),
                 error);
}

bool eval_apply_style_file(content::CapabilityHost& host,
                           const Module& module,
                           const Inst& inst,
                           SlotTable& slots,
                           std::string* error) {
  return ok_step(inst, ir::apply_style_file(host, bound_path(module, inst, slots, 0, 1)),
                 error);
}

bool eval_create_layer(content::CapabilityHost& host,
                       const Module& module,
                       const Inst& inst,
                       SlotTable& slots,
                       std::string* error) {
  return ok_step(inst,
                 ir::create_layer(host, arg_str_param(module, inst, slots, 0),
                                  arg_str_param(module, inst, slots, 1)),
                 error);
}

bool eval_remove_layer(content::CapabilityHost& host,
                       const Module& module,
                       const Inst& inst,
                       SlotTable& slots,
                       std::string* error) {
  return ok_step(inst, ir::remove_layer(host, arg_str_param(module, inst, slots, 0)),
                 error);
}

bool eval_set_layer_visible(content::CapabilityHost& host,
                            const Module& module,
                            const Inst& inst,
                            SlotTable& slots,
                            std::string* error) {
  return ok_step(inst,
                 ir::set_layer_visible(host, arg_str_param(module, inst, slots, 0),
                                       arg_bool_param(module, inst, slots, 1)),
                 error);
}

bool eval_select_layer(content::CapabilityHost& host,
                       const Module& module,
                       const Inst& inst,
                       SlotTable& slots,
                       std::string* error) {
  return ok_step(inst, ir::select_layer(host, arg_str_param(module, inst, slots, 0)),
                 error);
}

bool eval_update_feature_field(content::CapabilityHost& host,
                               const Module& module,
                               const Inst& inst,
                               SlotTable& slots,
                               std::string* error) {
  return ok_step(inst,
                 ir::update_feature_field(host, arg_str_param(module, inst, slots, 0),
                                          arg_str_param(module, inst, slots, 1),
                                          arg_str_param(module, inst, slots, 2)),
                 error);
}

bool eval_write_document(content::CapabilityHost& host,
                         const Module& module,
                         const Inst& inst,
                         SlotTable& slots,
                         std::string* error) {
  return ok_step(inst, ir::write_document(host, bound_path(module, inst, slots, 0, 1)),
                 error);
}

bool eval_clear_selection(content::CapabilityHost& host,
                          const Module&,
                          const Inst& inst,
                          SlotTable&,
                          std::string* error) {
  return ok_step(inst, ir::clear_selection(host), error);
}

constexpr Param kOpenParams[] = {
    {"path", 0, ArgKind::kPath, false, 0, ""},
    {"var", -1, ArgKind::kPath, false, 0, ""},
    {"fail_rc", -1, ArgKind::kI32, false, 0, nullptr},
};
constexpr Param kExportParams[] = {
    {"leaf", 0, ArgKind::kStr, false, 0, ""},
    {"frame", 1, ArgKind::kStr, false, 0, "document_extent"},
};
constexpr Param kPathParams[] = {
    {"path", 0, ArgKind::kPath, false, 0, ""},
    {"var", -1, ArgKind::kPath, false, 0, ""},
};
constexpr Param kCreateParams[] = {
    {"name", 0, ArgKind::kStr, false, 0, ""},
    {"geometry", 1, ArgKind::kStr, false, 0, "point"},
};
constexpr Param kIdParams[] = {
    {"id", 0, ArgKind::kStr, false, 0, ""},
};
constexpr Param kVisibleParams[] = {
    {"id", 0, ArgKind::kStr, false, 0, ""},
    {"visible", 1, ArgKind::kBool, false, 1, nullptr},
};
constexpr Param kFieldParams[] = {
    {"token", 0, ArgKind::kStr, false, 0, ""},
    {"field", 1, ArgKind::kStr, false, 0, ""},
    {"value", 2, ArgKind::kStr, false, 0, ""},
};

constexpr const char* const kOpenAliases[] = {"open_map", nullptr};

const OpDesc kOpenDocument{
    Op::kOpenDocument, "open_document", kOpenAliases, kOpenParams, 3, false,
    &eval_open_document,
};
const OpDesc kDocClear{
    Op::kDocClear, "doc_clear", nullptr, nullptr, 0, false, &eval_doc_clear,
};
const OpDesc kFitExtent{
    Op::kFitExtent, "fit_extent", nullptr, nullptr, 0, false, &eval_fit_extent,
};
const OpDesc kExportBmp{
    Op::kExportBmp, "export_bmp", nullptr, kExportParams, 2, false, &eval_export_bmp,
};
const OpDesc kApplyStyleFile{
    Op::kApplyStyleFile, "apply_style_file", nullptr, kPathParams, 2, false,
    &eval_apply_style_file,
};
const OpDesc kCreateLayer{
    Op::kCreateLayer, "create_layer", nullptr, kCreateParams, 2, false,
    &eval_create_layer,
};
const OpDesc kRemoveLayer{
    Op::kRemoveLayer, "remove_layer", nullptr, kIdParams, 1, false, &eval_remove_layer,
};
const OpDesc kSetLayerVisible{
    Op::kSetLayerVisible, "set_layer_visible", nullptr, kVisibleParams, 2, false,
    &eval_set_layer_visible,
};
const OpDesc kSelectLayer{
    Op::kSelectLayer, "select_layer", nullptr, kIdParams, 1, false, &eval_select_layer,
};
const OpDesc kUpdateFeatureField{
    Op::kUpdateFeatureField, "update_feature_field", nullptr, kFieldParams, 3, false,
    &eval_update_feature_field,
};
const OpDesc kWriteDocument{
    Op::kWriteDocument, "write_document", nullptr, kPathParams, 2, false,
    &eval_write_document,
};
const OpDesc kClearSelection{
    Op::kClearSelection, "clear_selection", nullptr, nullptr, 0, false,
    &eval_clear_selection,
};

}  // namespace

void fill_document_ops(const OpDesc** by_op) {
  if (!by_op) {
    return;
  }
  by_op[static_cast<std::size_t>(Op::kOpenDocument)] = &kOpenDocument;
  by_op[static_cast<std::size_t>(Op::kDocClear)] = &kDocClear;
  by_op[static_cast<std::size_t>(Op::kFitExtent)] = &kFitExtent;
  by_op[static_cast<std::size_t>(Op::kExportBmp)] = &kExportBmp;
  by_op[static_cast<std::size_t>(Op::kApplyStyleFile)] = &kApplyStyleFile;
  by_op[static_cast<std::size_t>(Op::kCreateLayer)] = &kCreateLayer;
  by_op[static_cast<std::size_t>(Op::kRemoveLayer)] = &kRemoveLayer;
  by_op[static_cast<std::size_t>(Op::kSetLayerVisible)] = &kSetLayerVisible;
  by_op[static_cast<std::size_t>(Op::kSelectLayer)] = &kSelectLayer;
  by_op[static_cast<std::size_t>(Op::kUpdateFeatureField)] = &kUpdateFeatureField;
  by_op[static_cast<std::size_t>(Op::kWriteDocument)] = &kWriteDocument;
  by_op[static_cast<std::size_t>(Op::kClearSelection)] = &kClearSelection;
}

}  // namespace code
}  // namespace content
