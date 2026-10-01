// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.
//
// Interact DSL language support: highlight via TextMate + Go to Definition
// into Interact.g4 (grammar keywords) and interact_dsl.cc (ops).
// Completions + snippets for common harness verbs.

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

/** Ops dispatched in shell/runtime/dsl/interact_dsl.cc (keep in sync). */
const OPS = [
  "pump",
  "select_map_tab",
  "catalog_tab",
  "inspector_tab",
  "mark",
  "wait_ready",
  "wait_map_ready",
  "load_sample",
  "load_china_sample",
  "detach_maps",
  "stop_map_timers",
  "clear_marks",
  "require_hwnd",
  "require_edit_host",
  "expect_host",
  "tool",
  "run_tool",
  "expect_tool",
  "expect_geom",
  "browse_stress",
  "expect_wheel_cursor",
  "map2d_run",
  "atmosphere_run",
  "plugin_run",
  "run_processing",
  "console_run",
  "resolve_data",
  "capture_path",
  "sidecar_path",
  "require_var",
  "doc_clear",
  "fit_extent",
  "export_bmp",
  "suppress_dialogs",
  "require_plugins",
  "apply_style_file",
  "invalidate_map2d",
  "analysis_set_frame",
  "analysis_export_frames",
  "window",
  "key",
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
  resolve_data: 'resolve_data(kind="plugin", leaf="file", as="var")',
  export_bmp: 'export_bmp(leaf="out.bmp", frame="…")',
  map2d_run: 'map2d_run("china"|"orthogrid"|…)',
  select_map_tab: "select_map_tab(index)",
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

function dslCcPath() {
  const root = workspaceRoot();
  if (!root) {
    return undefined;
  }
  // Canonical inproc executor (CapabilityHost verbs).
  const primary = path.join(
    root,
    "src",
    "app",
    "views",
    "shell",
    "runtime",
    "dsl",
    "interact_dsl.cc"
  );
  if (fs.existsSync(primary)) {
    return primary;
  }
  // Legacy showcase adapter path (pre-runtime move).
  return path.join(
    root,
    "src",
    "app",
    "views",
    "shell",
    "harness",
    "showcase",
    "ui",
    "interact_script.cc"
  );
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
      const cc = dslCcPath();
      const pos =
        findLine(cc, `if (op == "${word}")`) ||
        findLine(cc, `op == "${word}"`);
      if (cc && pos) {
        return new vscode.Location(vscode.Uri.file(cc), pos);
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
