// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.
//
// Preview *.ui.xml (approximate HTML Webview) and launch UiDesigner.exe.

"use strict";

const vscode = require("vscode");
const path = require("path");
const fs = require("fs");
const { spawn } = require("child_process");

/** @type {Map<string, vscode.WebviewPanel>} */
const panels = new Map();

function workspaceRoot() {
  const folders = vscode.workspace.workspaceFolders;
  if (!folders || folders.length === 0) {
    return undefined;
  }
  return folders[0].uri.fsPath;
}

function escapeHtml(s) {
  return String(s)
    .replace(/&/g, "&amp;")
    .replace(/</g, "&lt;")
    .replace(/>/g, "&gt;")
    .replace(/"/g, "&quot;");
}

function stripBomAndComments(xml) {
  let s = xml.replace(/^\uFEFF/, "");
  s = s.replace(/<\?xml[\s\S]*?\?>/gi, "");
  s = s.replace(/<!--[\s\S]*?-->/g, "");
  return s.trim();
}

function parseAttrs(raw) {
  const attrs = {};
  const re = /([A-Za-z_][\w:-]*)\s*=\s*("([^"]*)"|'([^']*)')/g;
  let m;
  while ((m = re.exec(raw)) !== null) {
    attrs[m[1]] = m[3] !== undefined ? m[3] : m[4];
  }
  return attrs;
}

/**
 * Minimal XML tree for Views markup subset (no namespaces / CDATA).
 * @returns {{ tag: string, attrs: object, children: object[], text: string } | null}
 */
function parseXmlFragment(xml) {
  const s = stripBomAndComments(xml);
  let i = 0;

  function skipWs() {
    while (i < s.length && /\s/.test(s[i])) {
      i += 1;
    }
  }

  function parseNode() {
    skipWs();
    if (s[i] !== "<") {
      return null;
    }
    if (s.startsWith("</", i)) {
      return null;
    }
    const tagMatch = /^<\/?([A-Za-z_][\w:-]*)/.exec(s.slice(i));
    if (!tagMatch) {
      return null;
    }
    const tag = tagMatch[1].toLowerCase();
    i += tagMatch[0].length;
    const attrsStart = i;
    let selfClose = false;
    while (i < s.length) {
      if (s.startsWith("/>", i)) {
        selfClose = true;
        i += 2;
        break;
      }
      if (s[i] === ">") {
        i += 1;
        break;
      }
      i += 1;
    }
    const attrs = parseAttrs(s.slice(attrsStart, i - (selfClose ? 2 : 1)));
    const node = { tag, attrs, children: [], text: "" };
    if (selfClose) {
      return node;
    }
    while (i < s.length) {
      skipWs();
      if (s.startsWith(`</${tag}`, i) || s.startsWith(`</${tagMatch[1]}`, i)) {
        const close = s.indexOf(">", i);
        i = close >= 0 ? close + 1 : s.length;
        break;
      }
      if (s[i] === "<") {
        const child = parseNode();
        if (child) {
          node.children.push(child);
        } else {
          i += 1;
        }
      } else {
        const next = s.indexOf("<", i);
        const chunk = next < 0 ? s.slice(i) : s.slice(i, next);
        node.text += chunk;
        i = next < 0 ? s.length : next;
      }
    }
    return node;
  }

  return parseNode();
}

function collectStyleSrcs(node, out) {
  if (!node) {
    return;
  }
  if (node.tag === "style" && node.attrs.src) {
    out.push(node.attrs.src);
  }
  for (const c of node.children) {
    collectStyleSrcs(c, out);
  }
}

function nodeToHtml(node) {
  if (!node) {
    return "";
  }
  const tag = node.tag;
  if (tag === "style" || tag === "ui") {
    if (tag === "ui") {
      return node.children.map(nodeToHtml).join("");
    }
    return "";
  }

  const id = node.attrs.id ? ` id="${escapeHtml(node.attrs.id)}"` : "";
  const cls = node.attrs.class
    ? ` class="${escapeHtml(node.attrs.class)}"`
    : "";
  const dataTag = ` data-tag="${escapeHtml(tag)}"`;
  const text = node.attrs.text || node.text.trim();

  if (tag === "vbox" || tag === "hbox") {
    const layoutClass = tag === "vbox" ? "ui-vbox" : "ui-hbox";
    const extra = cls ? cls.replace('class="', `class="${layoutClass} `) : ` class="${layoutClass}"`;
    const kids = node.children.map(nodeToHtml).join("");
    return `<div${id}${extra}${dataTag}>${kids}</div>`;
  }
  if (tag === "label") {
    return `<div${id}${cls || ' class="ui-label"'}${dataTag}>${escapeHtml(
      text
    )}</div>`;
  }
  if (tag === "button") {
    return `<button type="button"${id}${cls || ' class="ui-button"'}${dataTag}>${escapeHtml(
      text || "Button"
    )}</button>`;
  }
  if (tag === "textfield") {
    const value = escapeHtml(node.attrs.text || "");
    return `<input type="text"${id}${cls || ' class="ui-textfield"'}${dataTag} value="${value}"/>`;
  }

  // Unknown tags: keep as a plain container so nested markup still shows.
  const kids = node.children.map(nodeToHtml).join("");
  const body = kids || escapeHtml(text);
  const unknownCls = cls
    ? cls.replace('class="', 'class="ui-unknown ')
    : ' class="ui-unknown"';
  return `<div${id}${unknownCls}${dataTag}>${body}</div>`;
}

function loadCssBeside(xmlFsPath, relSrc) {
  try {
    const abs = path.normalize(path.join(path.dirname(xmlFsPath), relSrc));
    if (!fs.existsSync(abs)) {
      return `/* missing CSS: ${relSrc} */\n`;
    }
    return fs.readFileSync(abs, "utf8");
  } catch (e) {
    return `/* CSS read error: ${String(e)} */\n`;
  }
}

function buildPreviewHtml(xmlText, xmlFsPath) {
  const root = parseXmlFragment(xmlText);
  const styleSrcs = [];
  collectStyleSrcs(root, styleSrcs);
  const cssBlocks = styleSrcs.map((src) => loadCssBeside(xmlFsPath, src));
  const body = nodeToHtml(root) || "<p class='err'>Failed to parse UI markup.</p>";
  const title = (root && root.attrs && root.attrs.name) || path.basename(xmlFsPath);

  return `<!DOCTYPE html>
<html>
<head>
<meta charset="UTF-8"/>
<meta http-equiv="Content-Security-Policy" content="default-src 'none'; style-src 'unsafe-inline'; script-src 'unsafe-inline';"/>
<title>${escapeHtml(title)} — approx preview</title>
<style>
  html, body {
    margin: 0;
    padding: 0;
    background: #1e1e1e;
    color: #ddd;
    font: 13px/1.4 Segoe UI, sans-serif;
  }
  .chrome {
    display: flex;
    gap: 8px;
    align-items: center;
    padding: 8px 10px;
    border-bottom: 1px solid #333;
    background: #252526;
  }
  .chrome button {
    cursor: pointer;
    padding: 4px 10px;
  }
  .chrome .hint {
    opacity: 0.7;
    font-size: 12px;
  }
  .stage {
    padding: 16px;
    overflow: auto;
  }
  .frame {
    display: inline-block;
    background: #f0f0f0;
    color: #222;
    border: 1px solid #888;
    box-shadow: 0 2px 8px rgba(0,0,0,0.35);
  }
  .ui-vbox { display: flex; flex-direction: column; box-sizing: border-box; }
  .ui-hbox { display: flex; flex-direction: row; align-items: center; box-sizing: border-box; }
  .ui-label { box-sizing: border-box; }
  .ui-button {
    box-sizing: border-box;
    cursor: default;
    border: 1px solid #888;
    background: #e8e8e8;
    border-radius: 2px;
  }
  .ui-textfield {
    box-sizing: border-box;
    border: 1px solid #888;
    background: #fff;
    padding: 2px 6px;
  }
  .err { color: #f88; }
  ${cssBlocks.join("\n")}
</style>
</head>
<body>
  <div class="chrome">
    <strong>${escapeHtml(title)}</strong>
    <button id="openDesigner">Open in UiDesigner</button>
    <span class="hint">Approximate Webview — not Views/Skia. Save XML/CSS to refresh.</span>
  </div>
  <div class="stage">
    <div class="frame">${body}</div>
  </div>
  <script>
    const vscode = acquireVsCodeApi();
    document.getElementById('openDesigner').addEventListener('click', () => {
      vscode.postMessage({ type: 'openInDesigner' });
    });
  </script>
</body>
</html>`;
}

function isUiXmlUri(uri) {
  if (!uri || !uri.fsPath) {
    return false;
  }
  return uri.fsPath.toLowerCase().endsWith(".ui.xml");
}

function panelKey(uri) {
  return uri.toString();
}

function refreshPanel(panel, doc) {
  panel.webview.html = buildPreviewHtml(doc.getText(), doc.uri.fsPath);
  panel.title = `Preview: ${path.basename(doc.uri.fsPath)}`;
}

function showPreview(doc, column) {
  if (!doc || !isUiXmlUri(doc.uri)) {
    vscode.window.showWarningMessage("UI Markup preview expects a *.ui.xml file.");
    return;
  }
  const key = panelKey(doc.uri);
  let panel = panels.get(key);
  if (panel) {
    panel.reveal(column || vscode.ViewColumn.Beside);
    refreshPanel(panel, doc);
    return;
  }
  panel = vscode.window.createWebviewPanel(
    "moguUiMarkupPreview",
    `Preview: ${path.basename(doc.uri.fsPath)}`,
    column || vscode.ViewColumn.Beside,
    {
      enableScripts: true,
      retainContextWhenHidden: true,
      localResourceRoots: [vscode.Uri.file(path.dirname(doc.uri.fsPath))],
    }
  );
  panels.set(key, panel);
  panel.onDidDispose(() => {
    panels.delete(key);
  });
  panel.webview.onDidReceiveMessage((msg) => {
    if (msg && msg.type === "openInDesigner") {
      openInDesigner(doc.uri);
    }
  });
  refreshPanel(panel, doc);
}

function fileExists(p) {
  try {
    return !!(p && fs.existsSync(p) && fs.statSync(p).isFile());
  } catch (_) {
    return false;
  }
}

async function resolveDesignerExe() {
  const cfg = vscode.workspace.getConfiguration("mogu");
  const configured = (cfg.get("uiDesigner.path") || "").trim();
  if (configured && fileExists(configured)) {
    return configured;
  }

  const root = workspaceRoot();
  if (root) {
    const candidates = [
      path.join(root, "out", "Debug", "UiDesigner.exe"),
      path.join(root, "out", "Release", "UiDesigner.exe"),
    ];
    for (const c of candidates) {
      if (fileExists(c)) {
        return c;
      }
    }
  }

  const picked = await vscode.window.showOpenDialog({
    canSelectMany: false,
    openLabel: "Select UiDesigner.exe",
    filters: { Executable: ["exe"] },
  });
  if (!picked || picked.length === 0) {
    return undefined;
  }
  const chosen = picked[0].fsPath;
  if (!fileExists(chosen)) {
    return undefined;
  }
  await vscode.workspace
    .getConfiguration("mogu")
    .update("uiDesigner.path", chosen, vscode.ConfigurationTarget.Workspace);
  vscode.window.showInformationMessage(
    `Saved mogu.uiDesigner.path → ${chosen}`
  );
  return chosen;
}

async function openInDesigner(uri) {
  const target =
    uri && isUiXmlUri(uri)
      ? uri
      : vscode.window.activeTextEditor &&
        isUiXmlUri(vscode.window.activeTextEditor.document.uri)
        ? vscode.window.activeTextEditor.document.uri
        : undefined;
  if (!target) {
    vscode.window.showWarningMessage("Open a *.ui.xml file first.");
    return;
  }
  const exe = await resolveDesignerExe();
  if (!exe) {
    vscode.window.showErrorMessage(
      "UiDesigner.exe not found. Build //src/app/ui_designer or set mogu.uiDesigner.path."
    );
    return;
  }
  try {
    const child = spawn(exe, [target.fsPath], {
      detached: true,
      stdio: "ignore",
      windowsHide: false,
    });
    child.unref();
    vscode.window.setStatusBarMessage(
      `UiDesigner: ${path.basename(target.fsPath)}`,
      3000
    );
  } catch (e) {
    vscode.window.showErrorMessage(`Failed to launch UiDesigner: ${String(e)}`);
  }
}

function maybeAutoPreview(doc) {
  if (!doc || !isUiXmlUri(doc.uri)) {
    return;
  }
  const auto = vscode.workspace
    .getConfiguration("mogu")
    .get("uiMarkup.autoPreview", true);
  if (!auto) {
    return;
  }
  // Defer so the text editor claims the active column first.
  setTimeout(() => showPreview(doc, vscode.ViewColumn.Beside), 50);
}

function activate(context) {
  context.subscriptions.push(
    vscode.commands.registerCommand("mogu.uiMarkup.preview", () => {
      const ed = vscode.window.activeTextEditor;
      if (ed) {
        showPreview(ed.document);
      } else {
        vscode.window.showWarningMessage("No active editor.");
      }
    }),
    vscode.commands.registerCommand("mogu.uiMarkup.openInDesigner", () => {
      const ed = vscode.window.activeTextEditor;
      openInDesigner(ed ? ed.document.uri : undefined);
    }),
    vscode.workspace.onDidOpenTextDocument((doc) => {
      maybeAutoPreview(doc);
    }),
    vscode.workspace.onDidSaveTextDocument((doc) => {
      if (!isUiXmlUri(doc.uri) && !doc.uri.fsPath.toLowerCase().endsWith(".ui.css")) {
        return;
      }
      // Refresh any panel whose XML is this file, or whose CSS is beside an open panel.
      for (const [key, panel] of panels) {
        const xmlPath = vscode.Uri.parse(key).fsPath;
        if (
          doc.uri.fsPath === xmlPath ||
          path.dirname(doc.uri.fsPath) === path.dirname(xmlPath)
        ) {
          vscode.workspace.openTextDocument(vscode.Uri.file(xmlPath)).then(
            (xmlDoc) => refreshPanel(panel, xmlDoc),
            () => {}
          );
        }
      }
    }),
    vscode.workspace.onDidChangeTextDocument((e) => {
      if (!isUiXmlUri(e.document.uri)) {
        return;
      }
      const panel = panels.get(panelKey(e.document.uri));
      if (panel) {
        refreshPanel(panel, e.document);
      }
    })
  );

  for (const doc of vscode.workspace.textDocuments) {
    maybeAutoPreview(doc);
  }
}

function deactivate() {
  panels.clear();
}

module.exports = { activate, deactivate };
