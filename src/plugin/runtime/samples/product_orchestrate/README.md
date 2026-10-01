<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# Sample product orchestration (L3)

Python `kind=python` plugin that **orchestrates** existing L1 processing ids:

- `world3d.tin_from_xyz` — pick an XYZ/text file, assemble minimal JSON, `host.run_processing`
- `baogrid.create_orth_grid` — pick a grid-boundary text file, same pattern

It does **not** reimplement TIN / Laplace kernels. Builtin C++ UI under `src/plugin/product/world3d` and `src/plugin/product/orthogrid` remains the **reference** product surface; this sample shows the L3 path once L2 host seams (`pick_open_file`, `run_processing`, `show_message_box`) are bound.

## Enable

1. Ensure embeddable CPython is available (`SMT_HAS_PYTHON`) and Diagnostic Tools / PluginShell has initialized `PythonRuntime`.
2. Install this folder (with `plugin.json` + `plugin.py`) via Plugin Manager zip/index, **or** register the directory on `plugin::Registry` (`add_manifest` + set `directory` + `set_enabled`).
3. Trust unsigned packages if required, then enable `smartgis.sample_product_orchestrate`.
4. Tools menu: **Orchestrate TIN from XYZ** / **Orchestrate orth grid**. Cancel on the file picker is a no-op success.

Builtin DEM / baogrid plugins must already be enabled so the processing factories exist.
