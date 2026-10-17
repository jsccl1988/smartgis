// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

"use strict";

const path = require("path");
const vscode = require("vscode");
const { mcpModuleArgs, workspaceFolder, repoRoot } = require("./paths");
const { readCursorApiKeyFromEnv, stringEnv } = require("./env_key");

const SECRET_KEY = "smartgisDebug.cursorApiKey";

/**
 * Resolve API key: process/User env → SecretStorage → login/paste.
 * @param {import('vscode').ExtensionContext} context
 */
async function resolveApiKey(context) {
  const fromEnv = readCursorApiKeyFromEnv();
  if (fromEnv) {
    try {
      await context.secrets.store(SECRET_KEY, fromEnv);
    } catch (_) {
      /* ignore */
    }
    return fromEnv;
  }
  const stored = await context.secrets.get(SECRET_KEY);
  if (stored) {
    return stored;
  }
  return null;
}

/**
 * @param {import('vscode').ExtensionContext} context
 */
async function loginAndStore(context) {
  const pasted = await vscode.window.showInputBox({
    prompt:
      "Paste CURSOR_API_KEY (or set Windows User env and Reload Window). " +
      "WSL: export in ~/.bashrc then sync to Windows.",
    password: true,
    ignoreFocusOut: true,
    placeHolder: "crsr_…",
  });
  if (!pasted || !pasted.trim()) {
    throw new Error(
      "login cancelled — set User env CURSOR_API_KEY and Reload Window, " +
        "or run SmartGIS Debug: Login Cursor"
    );
  }
  const key = pasted.trim();
  await context.secrets.store(SECRET_KEY, key);
  process.env.CURSOR_API_KEY = key;
  return key;
}

/**
 * Prefer absolute python.exe; fall back to configured launcher.
 */
function resolvePythonCommand() {
  const configured = vscode.workspace
    .getConfiguration("smartgisDebug")
    .get("pythonPath");
  if (configured && String(configured).trim() && String(configured) !== "py") {
    return String(configured).trim();
  }
  if (process.platform === "win32") {
    const local = process.env.LOCALAPPDATA || "";
    const candidates = [
      path.join(local, "Programs", "Python", "Python312", "python.exe"),
      path.join(local, "Programs", "Python", "Python311", "python.exe"),
      path.join(local, "Programs", "Python", "Python313", "python.exe"),
    ];
    const fs = require("fs");
    for (const c of candidates) {
      if (fs.existsSync(c)) return c;
    }
  }
  return configured && String(configured).trim()
    ? String(configured).trim()
    : "py";
}

/**
 * @param {import('vscode').ExtensionContext} context
 * @param {string} prompt
 * @param {(chunk: string) => void} onDelta
 * @param {{ signal?: AbortSignal }} [opts]
 */
async function runConversationalDebug(context, prompt, onDelta, opts) {
  const signal = opts && opts.signal;
  let apiKey = await resolveApiKey(context);
  if (!apiKey) {
    apiKey = await loginAndStore(context);
  }

  let Agent;
  try {
    const mod = await import("@cursor/sdk");
    Agent = mod.Agent;
  } catch (err) {
    throw new Error(
      "Cannot load @cursor/sdk. Reinstall mogu.smartgis-debug VSIX. " +
        (err && err.message ? err.message : String(err))
    );
  }

  const cwd = workspaceFolder() || repoRoot(context);
  const modelId =
    vscode.workspace.getConfiguration("smartgisDebug").get("modelId") ||
    "composer-2.5";
  const py = resolvePythonCommand();
  const mcp = mcpModuleArgs(context);
  const mcpArgs =
    path.basename(py).toLowerCase() === "py.exe" ||
    path.basename(py).toLowerCase() === "py"
      ? ["-3", "-m", "debug.mcp"]
      : ["-m", "debug.mcp"];

  const systemHint =
    "You are the SmartGIS Debug assistant. Prefer MCP tools " +
    "(sg_ping, sg_log_tail, sg_cmd_exec, sg_diag_pack, sg_confirm, …) " +
    "for evidence. Only loopback DebugAgent. Never bypass confirm for " +
    "dangerous ops. Do not modify content/public APIs. " +
    "Reply briefly in Chinese unless the user writes in English.";

  if (signal && signal.aborted) {
    throw new Error("cancelled");
  }

  const agent = await Agent.create({
    apiKey,
    model: { id: modelId },
    local: { cwd },
    mcpServers: {
      "smartgis-debug": {
        type: "stdio",
        command: py,
        args: mcpArgs,
        env: stringEnv(mcp.env),
        cwd: mcp.cwd || cwd,
      },
    },
  });

  const run = await agent.send(systemHint + "\n\nUser: " + prompt);
  if (run && typeof run.stream === "function") {
    for await (const event of run.stream()) {
      if (signal && signal.aborted) {
        if (typeof run.cancel === "function") {
          try {
            await run.cancel();
          } catch (_) {
            /* ignore */
          }
        }
        throw new Error("cancelled");
      }
      if (event && event.type === "assistant" && event.message) {
        const content = event.message.content || [];
        for (const block of content) {
          if (block && block.type === "text" && block.text) {
            onDelta(block.text);
          }
        }
      } else if (event && event.type === "text" && event.text) {
        onDelta(event.text);
      }
    }
  }
  if (run && typeof run.wait === "function") {
    const result = await run.wait();
    if (result && result.result && typeof result.result === "string") {
      onDelta(result.result);
    }
    return result;
  }
  return run;
}

module.exports = {
  resolveApiKey,
  loginAndStore,
  runConversationalDebug,
  readCursorApiKeyFromEnv,
  SECRET_KEY,
};
