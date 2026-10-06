<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# Plugin report browser (WebView2) — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Shell-owned Report dock + `ReportBrowser` (v1 WebView2) so analysis plugins can open local HTML+JS report packs via Host API.

**Architecture:** Approach 1 — Views `ReportPanel` owns HWND + `plugin::ReportBrowser`; `PluginHost` bridges `open_report` / `post_to_report` / `close_report`. Static packs + optional JSON postMessage. No network navigation.

**Tech Stack:** C++23, Views, WebView2 loader (`//third_party/webview2`), GN/`build.bat`, Python Host bindings.

## Global Constraints

- Work on `master`; no feature branch.
- No Qt; no CEF product chrome revival; CEF only as optional later ReportBrowser backend.
- Compile via `build.bat` + `out/.build.lock`; set `SMARTGIS_BUILD_OWNER`.
- Namespaces ≤2 (`plugin`, `ui::views`, `content`); functions `snake_case`.
- Copyright 2026 The Mogu Authors; comments English.
- Soft-fail if WebView2 Runtime missing.

---

## File map

| Path | Responsibility |
| --- | --- |
| `third_party/webview2/` | Headers + WebView2Loader + GN |
| `src/plugin/runtime/browser/report_browser.h` | Abstract + allowlist helpers |
| `src/plugin/runtime/browser/fake_report_browser.*` | Test double |
| `src/plugin/runtime/browser/webview2_report_browser.*` | v1 backend |
| `src/plugin/runtime/browser/BUILD.gn` | Target + unit test |
| `src/ui/gis/shell/report_panel.*` | Inspector Report tab |
| `src/ui/resources/shell/report_panel.ui.*` | Markup |
| `src/content/public/plugin_host.h` + `plugin_host.cc` | Host bridge API |
| `src/app/views/ui/*` | Wire tab + set_report_bridge |
| `src/plugin/runtime/python/bindings.cc` | Python Host methods |
| `testing/data/plugin/report/sample/` | Sample index.html + data.json |

---

### Task 1 — Living § + WebView2 pin + ReportBrowser abstract

- [x] Append §report browser to plugin-host + §Report dock to views-shell
- [x] Vendor WebView2 headers/loader under `third_party/webview2`
- [x] `ReportBrowser` + `FakeReportBrowser` + path allowlist unit test green

### Task 2 — WebView2 backend + ReportPanel

- [x] `WebView2ReportBrowser` (navigate dir → file URL; block http(s); post_json)
- [x] `ReportPanel` inspector tab + native child HWND
- [x] Soft-fail status when Runtime missing

### Task 3 — Host bridge + Python + sample

- [x] `PluginHost` `set_report_bridge` / `open_report` / `post_to_report` / `close_report`
- [x] BrowserView installs bridge; show Report tab on open
- [x] Python bindings; sample pack under `testing/data/plugin/report/sample/`
- [x] `build.bat debug` targets green (`plugin_report_browser_test` OK; `SmartGisViews` link OK — contend on LNK1168 if exe locked)

---
