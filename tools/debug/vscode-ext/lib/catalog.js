// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

"use strict";

const fs = require("fs");
const vscode = require("vscode");
const { catalogPath, prefixesPath } = require("./paths");

/**
 * @param {import('vscode').ExtensionContext} context
 */
function loadCatalog(context) {
  const methodsPath = catalogPath(context);
  const prefPath = prefixesPath(context);
  let methods = {};
  let dangerous = [];
  let prefixes = [];
  try {
    const raw = JSON.parse(fs.readFileSync(methodsPath, "utf8"));
    methods = raw.methods || {};
    dangerous = Array.isArray(raw.dangerous) ? raw.dangerous : [];
  } catch (err) {
    console.warn("smartgis-debug: cannot read agent_methods.json", err);
  }
  try {
    const raw = JSON.parse(fs.readFileSync(prefPath, "utf8"));
    prefixes = Array.isArray(raw.prefixes) ? raw.prefixes : [];
  } catch (err) {
    console.warn("smartgis-debug: cannot read console_prefixes.json", err);
  }
  return { methods, dangerous, prefixes, methodsPath, prefPath };
}

/**
 * @param {import('vscode').ExtensionContext} context
 */
function registerCompletions(context) {
  const provider = {
    provideCompletionItems(document, position) {
      const line = document.lineAt(position).text.slice(0, position.character);
      const cat = loadCatalog(context);
      /** @type {import('vscode').CompletionItem[]} */
      const items = [];

      if (line.trimStart().startsWith(":") || line.includes(":")) {
        for (const prefix of cat.prefixes) {
          const item = new vscode.CompletionItem(
            prefix,
            vscode.CompletionItemKind.Keyword
          );
          item.detail = "DebugAgent console";
          if (
            prefix.startsWith(":py") ||
            prefix.startsWith(":sdbd sql") ||
            prefix.startsWith(":ui click") ||
            prefix.startsWith(":ui capture")
          ) {
            item.documentation = "Dangerous — needs :confirm or SG_DEBUG_ALLOW";
          }
          items.push(item);
        }
      }

      for (const name of Object.keys(cat.methods)) {
        const item = new vscode.CompletionItem(
          name,
          vscode.CompletionItemKind.Method
        );
        item.detail = "DebugAgent RPC";
        if (cat.dangerous.includes(name)) {
          item.documentation = "Dangerous — needs rpc.confirm / SG_DEBUG_ALLOW";
        }
        items.push(item);
      }
      return items;
    },
  };

  context.subscriptions.push(
    vscode.languages.registerCompletionItemProvider(
      [{ scheme: "file" }, { scheme: "untitled" }],
      provider,
      ":",
      "."
    )
  );
}

module.exports = { loadCatalog, registerCompletions };
