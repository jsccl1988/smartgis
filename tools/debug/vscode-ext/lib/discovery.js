// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

"use strict";

const fs = require("fs");
const os = require("os");
const path = require("path");
const vscode = require("vscode");

function defaultDiscoveryPath() {
  const override = vscode.workspace
    .getConfiguration("smartgisDebug")
    .get("discoveryPath");
  if (override && String(override).trim()) {
    return String(override).trim();
  }
  return path.join(os.tmpdir(), "smartgis-debug.json");
}

/**
 * @returns {{ ok: true, host: string, port: number, path: string, url: string }
 *         | { ok: false, path: string, error: string }}
 */
function loadDiscovery() {
  const filePath = defaultDiscoveryPath();
  try {
    if (!fs.existsSync(filePath)) {
      return { ok: false, path: filePath, error: "discovery file missing" };
    }
    const data = JSON.parse(fs.readFileSync(filePath, "utf8"));
    const host = String(data.host || "127.0.0.1");
    if (host !== "127.0.0.1" && host !== "localhost") {
      return {
        ok: false,
        path: filePath,
        error: `refusing non-loopback host: ${host}`,
      };
    }
    const port = Number(data.port);
    if (!Number.isInteger(port) || port <= 0 || port > 65535) {
      return { ok: false, path: filePath, error: `bad port: ${data.port}` };
    }
    return {
      ok: true,
      host: "127.0.0.1",
      port,
      path: filePath,
      url: `tcp://127.0.0.1:${port}`,
    };
  } catch (err) {
    return {
      ok: false,
      path: filePath,
      error: err && err.message ? err.message : String(err),
    };
  }
}

module.exports = { defaultDiscoveryPath, loadDiscovery };
