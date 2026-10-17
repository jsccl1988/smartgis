// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

"use strict";

const net = require("net");
const { loadDiscovery } = require("./discovery");

/**
 * One-shot NDJSON RPC call to DebugAgent.
 * @param {string} method
 * @param {object} [params]
 * @param {number} [timeoutMs]
 * @returns {Promise<object>}
 */
function callAgent(method, params = {}, timeoutMs = 8000) {
  const disc = loadDiscovery();
  if (!disc.ok) {
    return Promise.reject(new Error(disc.error + ` (${disc.path})`));
  }
  return new Promise((resolve, reject) => {
    const sock = net.createConnection({ host: disc.host, port: disc.port });
    let buf = "";
    let settled = false;
    const timer = setTimeout(() => {
      fail(new Error(`timeout calling ${method}`));
    }, timeoutMs);

    function fail(err) {
      if (settled) return;
      settled = true;
      clearTimeout(timer);
      sock.destroy();
      reject(err);
    }

    function ok(value) {
      if (settled) return;
      settled = true;
      clearTimeout(timer);
      sock.end();
      resolve(value);
    }

    sock.on("error", fail);
    sock.on("connect", () => {
      const req = JSON.stringify({
        id: 1,
        method,
        params: params || {},
      });
      sock.write(req + "\n");
    });
    sock.on("data", (chunk) => {
      buf += chunk.toString("utf8");
      let idx;
      while ((idx = buf.indexOf("\n")) >= 0) {
        const line = buf.slice(0, idx).trim();
        buf = buf.slice(idx + 1);
        if (!line) continue;
        let msg;
        try {
          msg = JSON.parse(line);
        } catch (err) {
          fail(err);
          return;
        }
        if (msg.event && msg.id === undefined) {
          continue; // push event; wait for response
        }
        if (msg.ok === false) {
          fail(new Error(msg.error || "agent error"));
          return;
        }
        if (msg.ok === true || msg.result !== undefined) {
          ok(msg.result !== undefined ? msg.result : msg);
          return;
        }
      }
    });
    sock.on("close", () => {
      if (!settled) {
        fail(new Error("connection closed before response"));
      }
    });
  });
}

module.exports = { callAgent };
