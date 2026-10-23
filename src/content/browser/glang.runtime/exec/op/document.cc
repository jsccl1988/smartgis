// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "content/browser/glang.runtime/exec/op/register.h"
#include "content/browser/capability/host.h"
#include "content/browser/glang.runtime/exec/op/install.h"


#include <tuple>

namespace content {
namespace code {
namespace {
bool apply_rc(content::CapabilityHost& host, int rc) {
  if (rc != 0) {
    host.fail_rc = rc;
    return false;
  }
  return true;
}

bool note_fail(content::CapabilityHost& host, bool ok, int fail_rc) {
  if (!ok && fail_rc != 0) {
    host.fail_rc = fail_rc;
  }
  return ok;
}

bool open_document(content::CapabilityHost& host, std::string_view path) {
  return !path.empty() && host.document.open_document &&
         host.document.open_document(std::string(path));
}

bool doc_clear(content::CapabilityHost& host) {
  return host.document.doc_clear && host.document.doc_clear();
}

bool fit_extent(content::CapabilityHost& host) {
  return host.document.fit_extent && host.document.fit_extent();
}

bool export_bmp(content::CapabilityHost& host,
                       std::string_view leaf,
                       std::string_view frame) {
  return host.document.export_bmp &&
         host.document.export_bmp(std::string(leaf), std::string(frame));
}

bool apply_style_file(content::CapabilityHost& host,
                             std::string_view path) {
  return !path.empty() && host.document.apply_style_file &&
         host.document.apply_style_file(std::string(path));
}

bool create_layer(content::CapabilityHost& host,
                         std::string_view name,
                         std::string_view geometry) {
  return !name.empty() && host.document.create_layer &&
         host.document.create_layer(std::string(name), std::string(geometry));
}

bool remove_layer(content::CapabilityHost& host, std::string_view id) {
  return !id.empty() && host.document.remove_layer &&
         host.document.remove_layer(std::string(id));
}

bool set_layer_visible(content::CapabilityHost& host,
                              std::string_view id,
                              bool visible) {
  return !id.empty() && host.document.set_layer_visible &&
         host.document.set_layer_visible(std::string(id), visible);
}

bool select_layer(content::CapabilityHost& host, std::string_view id) {
  return !id.empty() && host.document.select_layer &&
         host.document.select_layer(std::string(id));
}

int layer_count(content::CapabilityHost& host) {
  return host.document.layer_count ? host.document.layer_count() : -1;
}

int feature_count(content::CapabilityHost& host) {
  return host.document.feature_count ? host.document.feature_count() : -1;
}

bool update_feature_field(content::CapabilityHost& host,
                                 std::string_view token,
                                 std::string_view field,
                                 std::string_view value) {
  return !token.empty() && !field.empty() &&
         host.document.update_feature_field &&
         host.document.update_feature_field(std::string(token),
                                            std::string(field),
                                            std::string(value));
}

bool write_document(content::CapabilityHost& host, std::string_view path) {
  return !path.empty() && host.document.write_document &&
         host.document.write_document(std::string(path));
}

bool clear_selection(content::CapabilityHost& host) {
  return host.document.clear_selection && host.document.clear_selection();
}


bool eval_open_document(content::CapabilityHost& host,
                        const Module& module,
                        const Inst& inst,
                        SlotTable& slots,
                        std::string* error) {
  const std::string path = bound_path(module, inst, slots, 0, 1);
  const int fail_rc = arg_i32_param(module, inst, slots, 2);
  const bool opened = open_document(host, path);
  return ok_step(inst, note_fail(host, opened, fail_rc), error);
}

bool eval_doc_clear(content::CapabilityHost& host,
                    const Module&,
                    const Inst& inst,
                    SlotTable&,
                    std::string* error) {
  return ok_step(inst, doc_clear(host), error);
}

bool eval_fit_extent(content::CapabilityHost& host,
                     const Module&,
                     const Inst& inst,
                     SlotTable&,
                     std::string* error) {
  return ok_step(inst, fit_extent(host), error);
}

bool eval_export_bmp(content::CapabilityHost& host,
                     const Module& module,
                     const Inst& inst,
                     SlotTable& slots,
                     std::string* error) {
  return ok_step(inst,
                 export_bmp(host, arg_str_param(module, inst, slots, 0),
                                arg_str_param(module, inst, slots, 1)),
                 error);
}

bool eval_apply_style_file(content::CapabilityHost& host,
                           const Module& module,
                           const Inst& inst,
                           SlotTable& slots,
                           std::string* error) {
  return ok_step(inst, apply_style_file(host, bound_path(module, inst, slots, 0, 1)),
                 error);
}

bool eval_create_layer(content::CapabilityHost& host,
                       const Module& module,
                       const Inst& inst,
                       SlotTable& slots,
                       std::string* error) {
  return ok_step(inst,
                 create_layer(host, arg_str_param(module, inst, slots, 0),
                                  arg_str_param(module, inst, slots, 1)),
                 error);
}

bool eval_remove_layer(content::CapabilityHost& host,
                       const Module& module,
                       const Inst& inst,
                       SlotTable& slots,
                       std::string* error) {
  return ok_step(inst, remove_layer(host, arg_str_param(module, inst, slots, 0)),
                 error);
}

bool eval_set_layer_visible(content::CapabilityHost& host,
                            const Module& module,
                            const Inst& inst,
                            SlotTable& slots,
                            std::string* error) {
  return ok_step(inst,
                 set_layer_visible(host, arg_str_param(module, inst, slots, 0),
                                       arg_bool_param(module, inst, slots, 1)),
                 error);
}

bool eval_select_layer(content::CapabilityHost& host,
                       const Module& module,
                       const Inst& inst,
                       SlotTable& slots,
                       std::string* error) {
  return ok_step(inst, select_layer(host, arg_str_param(module, inst, slots, 0)),
                 error);
}

bool eval_update_feature_field(content::CapabilityHost& host,
                               const Module& module,
                               const Inst& inst,
                               SlotTable& slots,
                               std::string* error) {
  return ok_step(inst,
                 update_feature_field(host, arg_str_param(module, inst, slots, 0),
                                          arg_str_param(module, inst, slots, 1),
                                          arg_str_param(module, inst, slots, 2)),
                 error);
}

bool eval_write_document(content::CapabilityHost& host,
                         const Module& module,
                         const Inst& inst,
                         SlotTable& slots,
                         std::string* error) {
  return ok_step(inst, write_document(host, bound_path(module, inst, slots, 0, 1)),
                 error);
}

bool eval_clear_selection(content::CapabilityHost& host,
                          const Module&,
                          const Inst& inst,
                          SlotTable&,
                          std::string* error) {
  return ok_step(inst, clear_selection(host), error);
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

CONTENT_LANE_OP(OpenDocumentOp, Op::kOpenDocument, "open_document", kOpenAliases,
                kOpenParams, 3, false, eval_open_document);
CONTENT_LANE_OP(DocClearOp, Op::kDocClear, "doc_clear", nullptr, nullptr, 0, false,
                eval_doc_clear);
CONTENT_LANE_OP(FitExtentOp, Op::kFitExtent, "fit_extent", nullptr, nullptr, 0, false,
                eval_fit_extent);
CONTENT_LANE_OP(ExportBmpOp, Op::kExportBmp, "export_bmp", nullptr, kExportParams, 2,
                false, eval_export_bmp);
CONTENT_LANE_OP(ApplyStyleFileOp, Op::kApplyStyleFile, "apply_style_file", nullptr,
                kPathParams, 2, false, eval_apply_style_file);
CONTENT_LANE_OP(CreateLayerOp, Op::kCreateLayer, "create_layer", nullptr, kCreateParams,
                2, false, eval_create_layer);
CONTENT_LANE_OP(RemoveLayerOp, Op::kRemoveLayer, "remove_layer", nullptr, kIdParams, 1,
                false, eval_remove_layer);
CONTENT_LANE_OP(SetLayerVisibleOp, Op::kSetLayerVisible, "set_layer_visible", nullptr,
                kVisibleParams, 2, false, eval_set_layer_visible);
CONTENT_LANE_OP(SelectLayerOp, Op::kSelectLayer, "select_layer", nullptr, kIdParams, 1,
                false, eval_select_layer);
CONTENT_LANE_OP(UpdateFeatureFieldOp, Op::kUpdateFeatureField, "update_feature_field",
                nullptr, kFieldParams, 3, false, eval_update_feature_field);
CONTENT_LANE_OP(WriteDocumentOp, Op::kWriteDocument, "write_document", nullptr,
                kPathParams, 2, false, eval_write_document);
CONTENT_LANE_OP(ClearSelectionOp, Op::kClearSelection, "clear_selection", nullptr,
                nullptr, 0, false, eval_clear_selection);

using DocumentOps = std::tuple<OpenDocumentOp,
                               DocClearOp,
                               FitExtentOp,
                               ExportBmpOp,
                               ApplyStyleFileOp,
                               CreateLayerOp,
                               RemoveLayerOp,
                               SetLayerVisibleOp,
                               SelectLayerOp,
                               UpdateFeatureFieldOp,
                               WriteDocumentOp,
                               ClearSelectionOp>;

}  // namespace

void fill_document_ops(const OpDesc** by_op) {
  detail::install_ops<DocumentOps>(by_op);
}

}  // namespace code
}  // namespace content
