// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

// Language-facing seam for host-supplied verbs. The opcode slot is generic.
// The host registers the script name and the eval; the language does not.

#ifndef IL_RUNTIME_CODEGEN_OP_HOST_VERB_H_
#define IL_RUNTIME_CODEGEN_OP_HOST_VERB_H_

#include "content/browser/glang.runtime/language/code/op.h"

namespace content {
namespace code {

// Called once, before the opcode table is frozen. Null clears the installer.
void set_host_verb_installer(void (*install)());

// |desc| must outlive the process. Re-registering the same Op replaces it.
void register_host_verb(const OpDesc* desc);

}  // namespace code
}  // namespace content

#endif  // IL_RUNTIME_CODEGEN_OP_HOST_VERB_H_
