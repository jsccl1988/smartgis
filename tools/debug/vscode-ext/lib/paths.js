// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

"use strict";

const fs = require("fs");
const path = require("path");
const vscode = require("vscode");

/**
 * Resolve tools/debug whether the extension is loaded from the source tree
 * (Install from Location) or from a VSIX under ~/.cursor/extensions.
 * @param {import('vscode').ExtensionContext} context
 */
function debugRoot(context) {
  const besideExt = path.resolve(context.extensionPath, "..");
  if (fs.existsSync(path.join(besideExt, "agent_methods.json"))) {
    return besideExt;
  }
  const folders = vscode.workspace.workspaceFolders || [];
  for (const folder of folders) {
    const candidate = path.join(folder.uri.fsPath, "tools", "debug");
    if (fs.existsSync(path.join(candidate, "agent_methods.json"))) {
      return candidate;
    }
  }
  // Last resort: walk up from workspace / extension for tools/debug.
  const seeds = [
    ...folders.map((f) => f.uri.fsPath),
    context.extensionPath,
  ];
  for (const seed of seeds) {
    let cur = seed;
    for (let i = 0; i < 8; i++) {
      const candidate = path.join(cur, "tools", "debug");
      if (fs.existsSync(path.join(candidate, "agent_methods.json"))) {
        return candidate;
      }
      const parent = path.dirname(cur);
      if (parent === cur) break;
      cur = parent;
    }
  }
  return besideExt;
}

/** @param {import('vscode').ExtensionContext} context */
function repoRoot(context) {
  return path.resolve(debugRoot(context), "..", "..");
}

function catalogPath(context) {
  return path.join(debugRoot(context), "agent_methods.json");
}

function prefixesPath(context) {
  return path.join(debugRoot(context), "console_prefixes.json");
}

function sampleLaunchPath(context) {
  return path.join(debugRoot(context), "vscode", "launch.json");
}

function mcpModuleArgs(context) {
  const toolsDir = path.join(repoRoot(context), "tools");
  const dbg = debugRoot(context);
  const root = repoRoot(context);
  const pythonPath = [toolsDir, dbg, process.env.PYTHONPATH || ""]
    .filter(Boolean)
    .join(path.delimiter);
  /** @type {Record<string, string>} */
  const env = {};
  for (const [k, v] of Object.entries(process.env)) {
    if (typeof v === "string") env[k] = v;
  }
  env.PYTHONPATH = pythonPath;
  return {
    cwd: root,
    env,
    args: ["-3", "-m", "debug.mcp"],
  };
}

function workspaceFolder() {
  const folders = vscode.workspace.workspaceFolders;
  return folders && folders.length ? folders[0].uri.fsPath : undefined;
}

module.exports = {
  debugRoot,
  repoRoot,
  catalogPath,
  prefixesPath,
  sampleLaunchPath,
  mcpModuleArgs,
  workspaceFolder,
};
