// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "plugin/runtime/extension/bind.h"

#include "content/browser/glang.runtime/exec/py/ext_module.h"
#include "plugin/runtime/host/catalog/registry.h"
#include "plugin/runtime/extension/runtime.h"

namespace plugin {

void bind_registry_python_ext(Registry* registry, PythonRuntime* runtime) {
  if (!registry || !runtime) {
    return;
  }
  registry->set_python_starter(
      [runtime](const PluginRecord& rec, content::PluginHost* host) {
        const bool mounted =
            content::py::mount_extension(rec.manifest.id, rec.directory);
        if (!mounted) {
          // Soft: packs without a loadable ext/ still start; empty leaf is
          // created when the interpreter is up. Fail only when start fails.
        }
        const bool ok =
            runtime->start(rec.directory, rec.manifest.entry, host);
        if (!ok) {
          content::py::unmount_extension(rec.manifest.id);
        }
        return ok;
      },
      [runtime](const PluginRecord& rec) {
        runtime->stop(rec.directory);
        content::py::unmount_extension(rec.manifest.id);
      });
}

}  // namespace plugin
