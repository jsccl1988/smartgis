// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

"use strict";

const { execFileSync } = require("child_process");

/**
 * Resolve CURSOR_API_KEY for the extension host.
 * Order: process.env → Windows User env (registry) → null.
 * Cursor may have been started before setx/User env was written.
 */
function readCursorApiKeyFromEnv() {
  const fromProcess = process.env.CURSOR_API_KEY;
  if (fromProcess && String(fromProcess).trim()) {
    return String(fromProcess).trim();
  }
  if (process.platform === "win32") {
    try {
      const out = execFileSync(
        "powershell.exe",
        [
          "-NoProfile",
          "-Command",
          "[Environment]::GetEnvironmentVariable('CURSOR_API_KEY','User')",
        ],
        { encoding: "utf8", windowsHide: true, timeout: 8000 }
      ).trim();
      if (out) {
        process.env.CURSOR_API_KEY = out;
        return out;
      }
    } catch (_) {
      /* ignore */
    }
  }
  return null;
}

/** @param {NodeJS.ProcessEnv | Record<string, string | undefined>} env */
function stringEnv(env) {
  /** @type {Record<string, string>} */
  const out = {};
  for (const [k, v] of Object.entries(env || {})) {
    if (typeof v === "string") {
      out[k] = v;
    }
  }
  return out;
}

module.exports = { readCursorApiKeyFromEnv, stringEnv };
