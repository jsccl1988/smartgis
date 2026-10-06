// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.
//
// Interact DSL language support: highlight via TextMate + Go to Definition
// into Interact.g4 (grammar keywords) and il.runtime lower / ir (ops).
// Completions + snippets for CapabilityHost-aligned harness verbs.

"use strict";

const vscode = require("vscode");
const path = require("path");
const fs = require("fs");

const GRAMMAR_KEYWORDS = {
  script: "SCRIPT",
  seq: "SEQ",
  repeat: "REPEAT",
  chord: "CHORD",
  "@inproc": "ATINPROC",
  "@os": "ATOS",
  "@drivers": "ATDRIVERS",
  "@inject": "ATINJECT",
};

/** Ops lowered in il.runtime/codegen/lower (keep in sync with ir + lower_*). */
const OPS = [
  "pump",
  "mark",
  "clear_marks",
  "select_map_tab",
  "catalog_tab",
  "inspector_tab",
  "require_hwnd",
  "suppress_dialogs",
  "window",
  "key",
  "apply_ui_theme",
  "apply_scenario_panels",
  "layout_gate",
  "ui_present_capture",
  "console_pan_bench",
  "expect_shell_tree",
  "expect_layout_bounds",
  "wire_debug_agent",
  "debug_exec",
  "open_map",
  "doc_clear",
  "fit_extent",
  "export_bmp",
  "apply_style_file",
  "detach_maps",
  "stop_map_timers",
  "resume_map_timers",
  "invalidate_map2d",
  "tool",
  "run_tool",
  "activate_tool",
  "wait_ready",
  "wait_map_ready",
  "wait_viewport",
  "require_edit_host",
  "expect_host",
  "browse_stress",
  "capture_browse_still",
  "fps_bench",
  "fit_scene_box",
  "camera_fly",
  "expect_tool",
  "expect_geom",
  "expect_wheel_cursor",
  "expect_scene_visible",
  "expect_orbit_moved",
  "expect_map_hwnd_sync",
  "run_plugin_command",
  "run_processing",
  "resolve_data",
  "capture_path",
  "sidecar_path",
  "require_var",
  "require_plugins",
  "analysis_set_frame",
  "analysis_export_frames",
  "open_report",
  "post_to_report",
  "click",
  "rclick",
  "dblclick",
  "drag",
  "wheel",
  "path",
  "pan_burst",
  "wheel_burst",
];

const OP_HINTS = {
  pump: "pump(ms)",
  mark: 'mark("token")',
  path: "path(target, [x,y], ...)",
  pan_burst: "pan_burst(target, count=, x=, y=, dx=, dy=, pump_ms=)",
  wheel_burst: "wheel_burst(target, count=, x=, y=, delta=, pump_ms=)",
  run_processing: 'run_processing(id="op.id", args="{\\"k\\":\\"$var\\"}")',
  run_plugin_command: 'run_plugin_command("pack.scenario.name")',
  resolve_data: 'resolve_data(kind="plugin", leaf="file", as="var")',
  export_bmp: 'export_bmp(leaf="out.bmp", frame="…")',
  wait_viewport: 'wait_viewport(face="map"|"scene", ms=, frame=1)',
  fit_scene_box: "fit_scene_box()",
  camera_fly: 'camera_fly(mode="orbit"|"spherical", ms=1600, steps=20)',
  open_map: 'open_map(path=…|var="name")',
  select_map_tab: "select_map_tab(index)",
  apply_scenario_panels: 'apply_scenario_panels("shell"|"data"|…)',
  layout_gate: 'layout_gate("shell"|"interact"|…)',
  ui_present_capture: 'ui_present_capture("shell"|…)',
};

function workspaceRoot() {
  const folders = vscode.workspace.workspaceFolders;
  if (!folders || folders.length === 0) {
    return undefined;
  }
  return folders[0].uri.fsPath;
}

function g4Path() {
  const root = workspaceRoot();
  if (!root) {
    return undefined;
  }
  const product = path.join(
    root,
    "src",
    "app",
    "views",
    "il.runtime",
    "frontend",
    "Interact.g4"
  );
  if (fs.existsSync(product)) {
    return product;
  }
  return path.join(
    root,
    "testing",
    "tools",
    "harness",
    "_shared",
    "scripts",
    "grammar",
    "Interact.g4"
  );
}

function lowerSearchRoots() {
  const root = workspaceRoot();
  if (!root) {
    return [];
  }
  return [
    path.join(root, "src", "app", "views", "il.runtime", "codegen", "lower"),
    path.join(root, "src", "app", "views", "il.runtime", "ir"),
  ];
}

function findOpDefinition(word) {
  for (const dir of lowerSearchRoots()) {
    if (!fs.existsSync(dir)) {
      continue;
    }
    for (const leaf of fs.readdirSync(dir)) {
      if (!leaf.endsWith(".cc") && !leaf.endsWith(".h")) {
        continue;
      }
      const full = path.join(dir, leaf);
      const pos =
        findLine(full, `"${word}"`) || findLine(full, `ir::${word}`);
      if (pos) {
        return new vscode.Location(vscode.Uri.file(full), pos);
      }
    }
  }
  return undefined;
}

function findLine(filePath, needle) {
  if (!filePath || !fs.existsSync(filePath)) {
    return undefined;
  }
  const text = fs.readFileSync(filePath, "utf8");
  const lines = text.split(/\r?\n/);
  for (let i = 0; i < lines.length; ++i) {
    if (lines[i].includes(needle)) {
      return new vscode.Position(i, Math.max(0, lines[i].indexOf(needle)));
    }
  }
  return undefined;
}

function wordAt(document, position) {
  const range = document.getWordRangeAtPosition(
    position,
    /@?[A-Za-z_][A-Za-z0-9_]*/
  );
  if (!range) {
    return undefined;
  }
  return { range, word: document.getText(range) };
}

class InteractDefinitionProvider {
  provideDefinition(document, position) {
    const hit = wordAt(document, position);
    if (!hit) {
      return undefined;
    }
    const { word } = hit;

    if (Object.prototype.hasOwnProperty.call(GRAMMAR_KEYWORDS, word)) {
      const g4 = g4Path();
      const token = GRAMMAR_KEYWORDS[word];
      const pos = findLine(g4, `${token}`);
      if (g4 && pos) {
        return new vscode.Location(vscode.Uri.file(g4), pos);
      }
    }

    if (OPS.includes(word)) {
      const loc = findOpDefinition(word);
      if (loc) {
        return loc;
      }
    }

    if (/^[A-Za-z_][A-Za-z0-9_]*$/.test(word)) {
      const g4 = g4Path();
      const pos = findLine(g4, "callStmt");
      if (g4 && pos) {
        return new vscode.Location(vscode.Uri.file(g4), pos);
      }
    }

    return undefined;
  }
}

class InteractDocumentSymbolProvider {
  provideDocumentSymbols(document) {
    const symbols = [];
    const text = document.getText();
    const scriptRe = /script\s+"([^"]+)"/g;
    let m;
    while ((m = scriptRe.exec(text)) !== null) {
      const start = document.positionAt(m.index);
      symbols.push(
        new vscode.DocumentSymbol(
          m[1],
          "script",
          vscode.SymbolKind.Module,
          new vscode.Range(start, start),
          new vscode.Range(start, start)
        )
      );
    }
    const markRe = /mark\(\s*"([^"]+)"\s*\)/g;
    while ((m = markRe.exec(text)) !== null) {
      const start = document.positionAt(m.index);
      symbols.push(
        new vscode.DocumentSymbol(
          m[1],
          "mark",
          vscode.SymbolKind.Event,
          new vscode.Range(start, start),
          new vscode.Range(start, start)
        )
      );
    }
    return symbols;
  }
}

class InteractCompletionProvider {
  provideCompletionItems(document, position) {
    const line = document.lineAt(position).text;
    const prefix = line.slice(0, position.character);
    // Inside a string → no op completions.
    if ((prefix.match(/"/g) || []).length % 2 === 1) {
      return undefined;
    }
    const items = OPS.map((name) => {
      const item = new vscode.CompletionItem(
        name,
        vscode.CompletionItemKind.Function
      );
      item.detail = OP_HINTS[name] || `${name}(…)`;
      item.insertText = new vscode.SnippetString(`${name}($0)`);
      return item;
    });
    for (const kw of ["script", "seq", "repeat", "chord"]) {
      const item = new vscode.CompletionItem(
        kw,
        vscode.CompletionItemKind.Keyword
      );
      items.push(item);
    }
    for (const a of ["@inproc", "@os", "@drivers", "@inject"]) {
      const item = new vscode.CompletionItem(
        a,
        vscode.CompletionItemKind.Keyword
      );
      items.push(item);
    }
    return items;
  }
}

function activate(context) {
  const sel = { language: "interact" };
  context.subscriptions.push(
    vscode.languages.registerDefinitionProvider(
      sel,
      new InteractDefinitionProvider()
    ),
    vscode.languages.registerDocumentSymbolProvider(
      sel,
      new InteractDocumentSymbolProvider()
    ),
    vscode.languages.registerCompletionItemProvider(
      sel,
      new InteractCompletionProvider(),
      ".",
      "@"
    )
  );
}

function deactivate() {}

module.exports = { activate, deactivate, OPS };
