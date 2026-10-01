// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.
//
// SmartGIS VTS preview: open ASCII VTK StructuredGrid (*.vts) in a Three.js
// webview. Matches orthogrid3d write_vtk_structured output only (M0).

"use strict";

const vscode = require("vscode");
const fs = require("fs");
const path = require("path");

/**
 * Parse SmartGIS ASCII VTK XML StructuredGrid (.vts).
 * @param {string} text
 * @returns {{ nx: number, ny: number, nz: number, positions: number[], cellOrth: number[] | null }}
 */
function parse_vts_ascii(text) {
  if (!text || typeof text !== "string") {
    throw new Error("empty VTS");
  }
  if (!/type\s*=\s*"StructuredGrid"/i.test(text)) {
    throw new Error("not a VTK StructuredGrid (.vts)");
  }

  const extent_m =
    text.match(/WholeExtent\s*=\s*"([^"]+)"/i) ||
    text.match(/Piece[^>]*Extent\s*=\s*"([^"]+)"/i);
  if (!extent_m) {
    throw new Error("missing WholeExtent / Piece Extent");
  }
  const parts = extent_m[1]
    .trim()
    .split(/\s+/)
    .map((s) => parseInt(s, 10));
  if (parts.length !== 6 || parts.some((n) => Number.isNaN(n))) {
    throw new Error("bad Extent: " + extent_m[1]);
  }
  const nx = parts[1] - parts[0] + 1;
  const ny = parts[3] - parts[2] + 1;
  const nz = parts[5] - parts[4] + 1;
  if (nx < 1 || ny < 1 || nz < 1) {
    throw new Error("empty extent dimensions");
  }
  const expected = nx * ny * nz;

  const points_block = text.match(
    /<Points>[\s\S]*?<DataArray[^>]*>([\s\S]*?)<\/DataArray>[\s\S]*?<\/Points>/i
  );
  if (!points_block) {
    throw new Error("missing Points DataArray");
  }
  const nums = points_block[1]
    .trim()
    .split(/[\s,]+/)
    .filter((s) => s.length > 0)
    .map((s) => parseFloat(s));
  if (nums.length < expected * 3) {
    throw new Error(
      "point count mismatch: got " +
        Math.floor(nums.length / 3) +
        " need " +
        expected
    );
  }
  const positions = nums.slice(0, expected * 3);

  let cellOrth = null;
  const cell_m = text.match(
    /<DataArray[^>]*Name\s*=\s*"orthogonality"[^>]*>([\s\S]*?)<\/DataArray>/i
  );
  if (cell_m) {
    const cells = cell_m[1]
      .trim()
      .split(/[\s,]+/)
      .filter((s) => s.length > 0)
      .map((s) => parseFloat(s));
    if (cells.length > 0 && cells.every((n) => !Number.isNaN(n))) {
      cellOrth = cells;
    }
  }

  return { nx, ny, nz, positions, cellOrth };
}

/**
 * @param {vscode.Uri} extension_uri
 * @param {vscode.Webview} webview
 * @param {string} title
 */
function build_html(extension_uri, webview, title) {
  const media = vscode.Uri.joinPath(extension_uri, "media");
  const three_uri = webview.asWebviewUri(
    vscode.Uri.joinPath(media, "three.min.js")
  );
  const orbit_uri = webview.asWebviewUri(
    vscode.Uri.joinPath(media, "OrbitControls.js")
  );
  const preview_uri = webview.asWebviewUri(
    vscode.Uri.joinPath(media, "preview.js")
  );
  const csp = [
    "default-src 'none'",
    `style-src ${webview.cspSource} 'unsafe-inline'`,
    `script-src ${webview.cspSource}`,
  ].join("; ");

  return `<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8" />
  <meta http-equiv="Content-Security-Policy" content="${csp}" />
  <meta name="viewport" content="width=device-width, initial-scale=1.0" />
  <title>${escape_html(title)}</title>
  <style>
    html, body { margin: 0; height: 100%; overflow: hidden; background: #1a1d21; color: #c8cdd2; font: 12px/1.4 Consolas, monospace; }
    #hud { position: absolute; left: 10px; top: 8px; z-index: 2; pointer-events: none; opacity: 0.9; }
    #err { position: absolute; left: 10px; right: 10px; bottom: 10px; color: #ff8a80; white-space: pre-wrap; }
    canvas { display: block; }
  </style>
</head>
<body>
  <div id="hud">SmartGIS VTS · drag orbit · wheel zoom</div>
  <div id="err"></div>
  <script src="${three_uri}"></script>
  <script src="${orbit_uri}"></script>
  <script src="${preview_uri}"></script>
</body>
</html>`;
}

function escape_html(s) {
  return String(s)
    .replace(/&/g, "&amp;")
    .replace(/</g, "&lt;")
    .replace(/>/g, "&gt;")
    .replace(/"/g, "&quot;");
}

/**
 * @param {vscode.Uri} uri
 * @param {vscode.ExtensionContext} context
 */
async function open_preview(uri, context) {
  const file_path = uri.fsPath;
  let text;
  try {
    text = fs.readFileSync(file_path, "utf8");
  } catch (e) {
    vscode.window.showErrorMessage("VTS read failed: " + e.message);
    return;
  }

  let mesh;
  try {
    mesh = parse_vts_ascii(text);
  } catch (e) {
    vscode.window.showErrorMessage("VTS parse failed: " + e.message);
    return;
  }

  const title = "VTS: " + path.basename(file_path);
  const panel = vscode.window.createWebviewPanel(
    "smartgis.vtsPreview",
    title,
    vscode.ViewColumn.Beside,
    {
      enableScripts: true,
      retainContextWhenHidden: true,
      localResourceRoots: [
        vscode.Uri.joinPath(context.extensionUri, "media"),
      ],
    }
  );

  panel.webview.html = build_html(context.extensionUri, panel.webview, title);

  const send = () => {
    panel.webview.postMessage({
      type: "mesh",
      path: file_path,
      nx: mesh.nx,
      ny: mesh.ny,
      nz: mesh.nz,
      positions: mesh.positions,
      cellOrth: mesh.cellOrth,
    });
  };

  panel.webview.onDidReceiveMessage((msg) => {
    if (msg && msg.type === "ready") {
      send();
    }
  });

  // In case preview.js loads before the listener is ready.
  setTimeout(send, 200);
}

/**
 * @param {vscode.ExtensionContext} context
 */
function activate(context) {
  context.subscriptions.push(
    vscode.commands.registerCommand("smartgis.vtsPreview.open", async (uri) => {
      let target = uri;
      if (!target || !target.fsPath) {
        const ed = vscode.window.activeTextEditor;
        if (ed && ed.document.uri.fsPath.toLowerCase().endsWith(".vts")) {
          target = ed.document.uri;
        }
      }
      if (!target || !target.fsPath) {
        const picked = await vscode.window.showOpenDialog({
          canSelectMany: false,
          filters: { "VTK StructuredGrid": ["vts"] },
        });
        if (!picked || !picked[0]) {
          return;
        }
        target = picked[0];
      }
      await open_preview(target, context);
    })
  );
}

function deactivate() {}

module.exports = {
  activate,
  deactivate,
  parse_vts_ascii,
};
