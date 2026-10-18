<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# Workspace editor support (Interact DSL / UI Markup / ANTLR / VTS)

## `*.vts` — Three.js StructuredGrid preview

Install the in-repo VTS preview extension once:

```bat
testing\tools\harness\_shared\scripts\vscode\install_vscode_vts_preview.bat
```

Then **Developer: Reload Window**. Open e.g. `out/Debug/captures/plugin-showcase-orthogrid3d.vts` and run **SmartGIS: Preview VTS** (or right-click the file). ASCII VTK `StructuredGrid` from `orthogrid3d` only.

---

## `*.ui.xml` / `*.ui.css` — preview + UiDesigner

Install the in-repo UI Markup extension once:

```bat
testing\tools\harness\_shared\scripts\vscode\install_vscode_ui_markup.bat
```

Then **Developer: Reload Window**. Open e.g. `src/plugin/product/flood/resources/inundate.ui.xml`:

- Auto Webview **approximate** preview (flex layout + linked `.ui.css`; not Views/Skia)
- Editor title / Command Palette: **UI Markup: Preview**, **UI Markup: Open in UiDesigner**
- Designer path: setting `mogu.uiDesigner.path`, else `out/Debug/UiDesigner.exe`, then `out/Release/`; missing → file picker (writes workspace setting)
- Save XML/CSS (or edit XML) refreshes the Webview; UiDesigner keeps its own hot-reload

Build designer: `build.bat debug //src/app/ui_designer:ui_designer` (or group `UiDesigner`).

---

## `StatExpr.g4` — highlight

Install recommended extension **ANTLR4 grammar syntax support** (`mike-lischke.vscode-antlr4`):

**Prerequisite:** a JDK on `PATH` (`java -version` works). The extension runs `java` with no separate path setting; `spawn java ENOENT` means Cursor cannot find `java.exe` — install a JDK (e.g. Microsoft OpenJDK 21), put its `bin` on the user `PATH`, set `JAVA_HOME`, then **fully restart** Cursor.

1. Open the repo in VS Code / Cursor.
2. Accept the workspace recommendation prompt, **or** Command Palette → `Extensions: Show Recommended Extensions`.
3. Open `src/gis/stat/StatExpr.g4`.

Harness scripts are Python (`.py`). There is no Interact DSL or `.il` language extension.
