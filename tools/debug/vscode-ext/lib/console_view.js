// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

"use strict";

const vscode = require("vscode");
const { callAgent } = require("./agent_tcp");
const { loadDiscovery } = require("./discovery");

class ConsoleViewProvider {
  /**
   * @param {import('vscode').ExtensionContext} context
   */
  constructor(context) {
    this._context = context;
  }

  /**
   * @param {import('vscode').WebviewView} webviewView
   */
  resolveWebviewView(webviewView) {
    this._view = webviewView;
    webviewView.webview.options = { enableScripts: true };
    webviewView.webview.html = this._html();
    webviewView.webview.onDidReceiveMessage(async (msg) => {
      if (!msg || !msg.type) return;
      try {
        if (msg.type === "refresh") {
          const disc = loadDiscovery();
          this._post({ type: "discovery", disc });
          return;
        }
        if (msg.type === "ping") {
          const result = await callAgent("ping");
          this._post({ type: "result", label: "ping", result });
          return;
        }
        if (msg.type === "confirm") {
          const result = await callAgent("rpc.confirm");
          this._post({ type: "result", label: "rpc.confirm", result });
          return;
        }
        if (msg.type === "logTail") {
          const result = await callAgent("log.tail", { n: Number(msg.n) || 80 });
          this._post({ type: "result", label: "log.tail", result });
          return;
        }
        if (msg.type === "exec") {
          let line = String(msg.line || "").trim();
          if (!line) return;
          if (!line.startsWith(":")) line = ":" + line;
          const dangerous =
            line.startsWith(":py") ||
            line.startsWith(":sdbd sql") ||
            line.startsWith(":ui click") ||
            line.startsWith(":ui capture");
          if (dangerous) {
            const ok = await vscode.window.showWarningMessage(
              `Dangerous command ${line}. Continue? (session may still need :confirm)`,
              { modal: true },
              "Run"
            );
            if (ok !== "Run") return;
          }
          const result = await callAgent("cmd.exec", { line });
          this._post({ type: "result", label: line, result });
        }
      } catch (err) {
        this._post({
          type: "error",
          error: err && err.message ? err.message : String(err),
        });
      }
    });
    const disc = loadDiscovery();
    this._post({ type: "discovery", disc });
  }

  _post(msg) {
    if (this._view) {
      this._view.webview.postMessage(msg);
    }
  }

  _html() {
    return `<!DOCTYPE html>
<html lang="zh-CN">
<head>
<meta charset="utf-8"/>
<style>
  body { font: 12px/1.4 Consolas, "Cascadia Mono", monospace; color: var(--vscode-foreground);
    background: var(--vscode-sideBar-background); margin: 0; padding: 8px; }
  .row { display: flex; gap: 4px; margin-bottom: 6px; flex-wrap: wrap; }
  button, input { font: inherit; }
  input { flex: 1; min-width: 120px; background: var(--vscode-input-background);
    color: var(--vscode-input-foreground); border: 1px solid var(--vscode-input-border, #555); padding: 4px; }
  button { background: var(--vscode-button-background); color: var(--vscode-button-foreground);
    border: none; padding: 4px 8px; cursor: pointer; }
  #status { color: var(--vscode-descriptionForeground); margin-bottom: 6px; }
  #out { white-space: pre-wrap; word-break: break-word; max-height: 60vh; overflow: auto;
    border-top: 1px solid var(--vscode-panel-border, #444); padding-top: 6px; }
  .err { color: var(--vscode-errorForeground); }
</style>
</head>
<body>
  <div id="status">discovery…</div>
  <div class="row">
    <button id="refresh">Refresh</button>
    <button id="ping">Ping</button>
    <button id="confirm">Confirm</button>
    <button id="log">log.tail</button>
  </div>
  <div class="row">
    <input id="line" placeholder=":help" />
    <button id="run">Run</button>
  </div>
  <div id="out"></div>
<script>
  const vscode = acquireVsCodeApi();
  const status = document.getElementById('status');
  const out = document.getElementById('out');
  function append(text, cls) {
    const d = document.createElement('div');
    if (cls) d.className = cls;
    d.textContent = text;
    out.appendChild(d);
    out.scrollTop = out.scrollHeight;
  }
  document.getElementById('refresh').onclick = () => vscode.postMessage({ type: 'refresh' });
  document.getElementById('ping').onclick = () => vscode.postMessage({ type: 'ping' });
  document.getElementById('confirm').onclick = () => vscode.postMessage({ type: 'confirm' });
  document.getElementById('log').onclick = () => vscode.postMessage({ type: 'logTail', n: 80 });
  document.getElementById('run').onclick = () => {
    const line = document.getElementById('line').value;
    vscode.postMessage({ type: 'exec', line });
  };
  document.getElementById('line').addEventListener('keydown', (e) => {
    if (e.key === 'Enter') document.getElementById('run').click();
  });
  window.addEventListener('message', (e) => {
    const msg = e.data;
    if (msg.type === 'discovery') {
      const d = msg.disc;
      status.textContent = d.ok
        ? ('Agent ' + d.url + '  (' + d.path + ')')
        : ('No agent: ' + d.error + '  (' + d.path + ')');
    } else if (msg.type === 'result') {
      append(msg.label + '\\n' + JSON.stringify(msg.result, null, 2));
    } else if (msg.type === 'error') {
      append(msg.error, 'err');
    }
  });
  vscode.postMessage({ type: 'refresh' });
</script>
</body>
</html>`;
  }
}

module.exports = { ConsoleViewProvider };
