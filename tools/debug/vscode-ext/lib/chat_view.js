// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

"use strict";

const {
  runConversationalDebug,
  resolveApiKey,
  readCursorApiKeyFromEnv,
} = require("./cursor_agent");
const { loadDiscovery } = require("./discovery");

class ChatViewProvider {
  /**
   * @param {import('vscode').ExtensionContext} context
   */
  constructor(context) {
    this._context = context;
    this._busy = false;
    /** @type {AbortController | null} */
    this._abort = null;
  }

  /**
   * @param {import('vscode').WebviewView} webviewView
   */
  resolveWebviewView(webviewView) {
    this._view = webviewView;
    this._busy = false;
    this._abort = null;
    webviewView.webview.options = { enableScripts: true };
    webviewView.webview.html = this._html();
    webviewView.webview.onDidReceiveMessage(async (msg) => {
      if (!msg || !msg.type) return;
      if (msg.type === "status") {
        await this._pushStatus();
        return;
      }
      if (msg.type === "cancel") {
        if (this._abort) {
          this._abort.abort();
        }
        this._busy = false;
        this._post({ type: "error", error: "cancelled" });
        this._post({ type: "assistantEnd" });
        return;
      }
      if (msg.type !== "send") return;
      if (this._busy) {
        this._post({
          type: "error",
          error: "agent busy — press Cancel, then retry",
        });
        return;
      }
      const text = String(msg.text || "").trim();
      if (!text) return;
      this._busy = true;
      this._abort = new AbortController();
      this._post({ type: "user", text });
      this._post({ type: "assistantStart" });
      const watchdog = setTimeout(() => {
        if (this._abort) this._abort.abort();
      }, 180000);
      try {
        await runConversationalDebug(
          this._context,
          text,
          (chunk) => {
            this._post({ type: "assistantDelta", text: chunk });
          },
          { signal: this._abort.signal }
        );
        this._post({ type: "assistantEnd" });
      } catch (err) {
        this._post({
          type: "error",
          error: err && err.message ? err.message : String(err),
        });
        this._post({ type: "assistantEnd" });
      } finally {
        clearTimeout(watchdog);
        this._busy = false;
        this._abort = null;
      }
    });
    void this._pushStatus();
  }

  async _pushStatus() {
    const disc = loadDiscovery();
    let auth = "no key";
    try {
      const key =
        readCursorApiKeyFromEnv() || (await resolveApiKey(this._context));
      if (key) {
        auth = `key ok (${key.slice(0, 8)}…)`;
      }
    } catch (_) {
      auth = "key error";
    }
    const agent = disc.ok ? disc.url : `no agent (${disc.error})`;
    this._post({ type: "status", text: `Auth: ${auth} | Agent: ${agent}` });
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
  body { font: 12px/1.45 "Segoe UI", sans-serif; color: var(--vscode-foreground);
    background: var(--vscode-sideBar-background); margin: 0; padding: 8px; display: flex;
    flex-direction: column; height: 100vh; box-sizing: border-box; }
  #log { flex: 1; overflow: auto; white-space: pre-wrap; word-break: break-word; margin-bottom: 8px; }
  .user { color: var(--vscode-textLink-foreground); margin: 6px 0; }
  .asst { margin: 6px 0; }
  .err { color: var(--vscode-errorForeground); }
  .status { color: var(--vscode-descriptionForeground); font-size: 11px; margin-bottom: 6px; }
  .row { display: flex; gap: 4px; }
  textarea { flex: 1; min-height: 52px; resize: vertical; font: inherit;
    background: var(--vscode-input-background); color: var(--vscode-input-foreground);
    border: 1px solid var(--vscode-input-border, #555); padding: 4px; }
  button { background: var(--vscode-button-background); color: var(--vscode-button-foreground);
    border: none; padding: 6px 10px; cursor: pointer; }
  button.secondary { background: var(--vscode-button-secondaryBackground);
    color: var(--vscode-button-secondaryForeground); }
</style>
</head>
<body>
  <div id="status" class="status">加载状态…</div>
  <div id="log"></div>
  <div class="row">
    <textarea id="input" placeholder="例如：Agent 是否在听？拉最近 log 并解释报错"></textarea>
    <button id="send">Send</button>
    <button id="cancel" class="secondary">Cancel</button>
  </div>
<script>
  const vscode = acquireVsCodeApi();
  const log = document.getElementById('log');
  const status = document.getElementById('status');
  let currentAsst = null;
  function add(cls, text) {
    const d = document.createElement('div');
    d.className = cls;
    d.textContent = text;
    log.appendChild(d);
    log.scrollTop = log.scrollHeight;
    return d;
  }
  document.getElementById('send').onclick = () => {
    const text = document.getElementById('input').value;
    document.getElementById('input').value = '';
    vscode.postMessage({ type: 'send', text });
  };
  document.getElementById('cancel').onclick = () => {
    vscode.postMessage({ type: 'cancel' });
  };
  window.addEventListener('message', (e) => {
    const msg = e.data;
    if (msg.type === 'status') status.textContent = msg.text;
    else if (msg.type === 'user') add('user', 'You: ' + msg.text);
    else if (msg.type === 'assistantStart') currentAsst = add('asst', '');
    else if (msg.type === 'assistantDelta' && currentAsst) {
      currentAsst.textContent += msg.text;
      log.scrollTop = log.scrollHeight;
    } else if (msg.type === 'assistantEnd') currentAsst = null;
    else if (msg.type === 'error') add('err', msg.error);
  });
  vscode.postMessage({ type: 'status' });
</script>
</body>
</html>`;
  }
}

module.exports = { ChatViewProvider };
