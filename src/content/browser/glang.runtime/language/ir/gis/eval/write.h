// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

// Geometry IR call and WKT write-back. Callers already decoded operands.

#ifndef CONTENT_BROWSER_GLANG_RUNTIME_IR_GIS_EVAL_WRITE_H_
#define CONTENT_BROWSER_GLANG_RUNTIME_IR_GIS_EVAL_WRITE_H_

#include "gis/analysis/ops/ir/geometry_ir.h"

#include <string>

class OGRGeometry;

namespace content {
namespace ir {
namespace detail {

// Copies the source coordinate dimension onto a geometry result and emits WKT.
bool write_geometry(const OGRGeometry& src,
                    gis::detail::IrValue value,
                    std::string* out,
                    std::string* error);

// Evaluates one geometry-IR id. A missing node yields an empty IrValue.
gis::detail::IrValue eval_env(const char* id, const gis::detail::IrEnv& env);

gis::detail::IrValue eval_geometry(const char* id,
                                   OGRGeometry* input,
                                   OGRGeometry* other,
                                   double number);

}  // namespace detail
}  // namespace ir
}  // namespace content

#endif  // CONTENT_BROWSER_GLANG_RUNTIME_IR_GIS_EVAL_WRITE_H_
