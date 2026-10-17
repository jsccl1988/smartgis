// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.
//
// SmartGIS DebugAgent VS Code / Cursor extension (P0 catalog + P1 console + P2 chat).

"use strict";

const fs = require("fs");
const path = require("path");
const vscode = require("vscode");
const { registerCompletions, loadCatalog } = require("./lib/catalog");
const { loadDiscovery, defaultDiscoveryPath } = require("./lib/discovery");
const { callAgent } = require("./lib/agent_tcp");
const { ConsoleViewProvider } = require("./lib/console_view");
const { ChatViewProvider } = require("./lib/chat_view");
const { loginAndStore, resolveApiKey } = require("./lib/cursor_agent");
const { readCursorApiKeyFromEnv } = require("./lib/env_key");
const { sampleLaunchPath, catalogPath } = require("./lib/paths");

/** @param {import('vscode').ExtensionContext} context */
function activate(context) {
  // Seed SecretStorage from User env (Cursor may predate setx).
  void resolveApiKey(context).catch(() => {});
  if (!process.env.CURSOR_API_KEY) {
    const k = readCursorApiKeyFromEnv();
    if (k) process.env.CURSOR_API_KEY = k;
  }

  const status = vscode.window.createStatusBarItem(
    vscode.StatusBarAlignment.Left,
    50
  );
  status.command = "smartgisDebug.refreshDiscovery";
  status.tooltip = "SmartGIS DebugAgent discovery";
  context.subscriptions.push(status);

  function refreshStatus() {
    const disc = loadDiscovery();
    if (disc.ok) {
      status.text = `$(bug) SG Agent :${disc.port}`;
      status.backgroundColor = undefined;
    } else {
      status.text = "$(bug) SG Agent —";
      status.backgroundColor = new vscode.ThemeColor(
        "statusBarItem.warningBackground"
      );
    }
    status.show();
  }

  refreshStatus();
  const timer = setInterval(refreshStatus, 5000);
  context.subscriptions.push({ dispose: () => clearInterval(timer) });

  registerCompletions(context);

  context.subscriptions.push(
    vscode.window.registerWebviewViewProvider(
      "smartgisDebug.consoleView",
      new ConsoleViewProvider(context)
    ),
    vscode.window.registerWebviewViewProvider(
      "smartgisDebug.chatView",
      new ChatViewProvider(context)
    )
  );

  context.subscriptions.push(
    vscode.commands.registerCommand("smartgisDebug.refreshDiscovery", () => {
      refreshStatus();
      const disc = loadDiscovery();
      vscode.window.showInformationMessage(
        disc.ok
          ? `DebugAgent at ${disc.url}`
          : `No Agent: ${disc.error} (${defaultDiscoveryPath()})`
      );
    }),
    vscode.commands.registerCommand("smartgisDebug.openCatalog", async () => {
      const uri = vscode.Uri.file(catalogPath(context));
      await vscode.window.showTextDocument(uri);
    }),
    vscode.commands.registerCommand("smartgisDebug.ping", async () => {
      try {
        const result = await callAgent("ping");
        vscode.window.showInformationMessage(
          "ping ok: " + JSON.stringify(result)
        );
      } catch (err) {
        vscode.window.showErrorMessage(
          err && err.message ? err.message : String(err)
        );
      }
    }),
    vscode.commands.registerCommand("smartgisDebug.confirm", async () => {
      try {
        const result = await callAgent("rpc.confirm");
        vscode.window.showInformationMessage(
          "confirm: " + JSON.stringify(result)
        );
      } catch (err) {
        vscode.window.showErrorMessage(
          err && err.message ? err.message : String(err)
        );
      }
    }),
    vscode.commands.registerCommand("smartgisDebug.openConsole", async () => {
      await vscode.commands.executeCommand(
        "smartgisDebug.consoleView.focus"
      );
    }),
    vscode.commands.registerCommand("smartgisDebug.openChat", async () => {
      await vscode.commands.executeCommand("smartgisDebug.chatView.focus");
    }),
    vscode.commands.registerCommand("smartgisDebug.loginCursor", async () => {
      try {
        await loginAndStore(context);
        vscode.window.showInformationMessage("Cursor API key stored.");
      } catch (err) {
        vscode.window.showErrorMessage(
          err && err.message ? err.message : String(err)
        );
      }
    }),
    vscode.commands.registerCommand("smartgisDebug.mergeLaunchJson", async () => {
      const folders = vscode.workspace.workspaceFolders;
      if (!folders || !folders.length) {
        vscode.window.showErrorMessage("No workspace folder");
        return;
      }
      const sample = JSON.parse(
        fs.readFileSync(sampleLaunchPath(context), "utf8")
      );
      const destDir = path.join(folders[0].uri.fsPath, ".vscode");
      const dest = path.join(destDir, "launch.json");
      fs.mkdirSync(destDir, { recursive: true });
      let destObj = { version: "0.2.0", configurations: [] };
      if (fs.existsSync(dest)) {
        try {
          destObj = JSON.parse(fs.readFileSync(dest, "utf8"));
        } catch (err) {
          vscode.window.showErrorMessage(
            "Existing launch.json is invalid JSON; refuse to merge."
          );
          return;
        }
      }
      if (!Array.isArray(destObj.configurations)) {
        destObj.configurations = [];
      }
      const sampleCfgs = sample.configurations || [];
      let added = 0;
      for (const cfg of sampleCfgs) {
        const exists = destObj.configurations.some(
          (c) => c && c.name === cfg.name
        );
        if (!exists) {
          destObj.configurations.push(cfg);
          added += 1;
        }
      }
      fs.writeFileSync(dest, JSON.stringify(destObj, null, 2) + "\n", "utf8");
      vscode.window.showInformationMessage(
        added
          ? `Merged ${added} launch config(s) into .vscode/launch.json`
          : "Sample launch configs already present"
      );
    })
  );

  // Touch catalog once so activate fails loudly if paths are wrong.
  loadCatalog(context);
}

function deactivate() {}

module.exports = { activate, deactivate };
