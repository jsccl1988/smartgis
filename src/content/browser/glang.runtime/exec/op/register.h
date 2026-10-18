// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

// Opcode table entry. find_op / ensure_ops_registered live in op.h.
// Lane translation units fill the table; they are not a public script API.

#ifndef IL_RUNTIME_CODEGEN_OP_REGISTER_H_
#define IL_RUNTIME_CODEGEN_OP_REGISTER_H_

#include "content/browser/glang.runtime/language/code/op.h"

namespace content {
namespace code {

void fill_document_ops(const OpDesc** by_op);
void fill_view_ops(const OpDesc** by_op);
void fill_plugin_ops(const OpDesc** by_op);
void fill_horizon_ops(const OpDesc** by_op);
void fill_gesture_ops(const OpDesc** by_op);
void fill_buffer_op(const OpDesc** by_op);
void fill_gis_ops(const OpDesc** by_op);

}  // namespace code
}  // namespace content

#endif  // IL_RUNTIME_CODEGEN_OP_REGISTER_H_
