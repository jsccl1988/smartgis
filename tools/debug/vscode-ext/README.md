<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# SmartGIS Debug（VS Code / Cursor 扩展）

面向 `content::DebugAgent` 的编辑器扩展：**补全 / 侧栏控制台 / Cursor SDK 对话式 debug**。  
协议仍是 loopback NDJSON；危险操作不绕过 `rpc.confirm` / `SG_DEBUG_ALLOW`。

Living design: `docs/superpowers/specs/2026-09-27-views-desktop-shell-design.md` §Debug Console **D8**.

## 安装

在仓库根目录：

```bat
cd tools\debug\vscode-ext
npm install
```

Cursor / VS Code：

1. **Extensions: Install from Location…** → 选本目录 `tools/debug/vscode-ext`，或  
2. `npx vsce package` 后安装生成的 `.vsix`。

推荐把本扩展路径加入工作区推荐（可选）。

## 使用

| 阶段 | 做什么 |
| --- | --- |
| P0 | 状态栏看 discovery；命令面板 `SmartGIS Debug: *`；输入 `:` 补全 console / RPC |
| P1 | 活动栏 **SmartGIS Debug → Console**：Ping / Confirm / `:cmd` / `log.tail` |
| P2 | **Chat**：需 `CURSOR_API_KEY` 或 `SmartGIS Debug: Login Cursor`；MCP 为 `py -3 -m debug.mcp` |

产品侧启用 Agent（Debug 构建默认开；Release 用 `SG_DEBUG=1` / `--debug-console`）。  
Discovery：`%TEMP%\smartgis-debug.json`。

## MCP（给 SDK / 其它宿主）

```bat
set PYTHONPATH=tools;tools\debug
py -3 -m debug.mcp
```

stdio + `Content-Length` 帧；工具名 `sg_ping` / `sg_cmd_exec` / `sg_log_tail` / …

## 鉴权

1. Windows **User** 环境变量 `CURSOR_API_KEY`（扩展会主动读 User 级，不要求 Cursor 重启后才有 process env）  
2. 否则 SecretStorage / 命令 **Login Cursor** 粘贴 key  
3. 从 WSL 同步示例：

```powershell
$line = (wsl -- bash -c "grep -h '^export CURSOR_API_KEY=' ~/.bashrc ~/.profile 2>/dev/null | head -1")
$key = ($line -replace '^export CURSOR_API_KEY=','').Trim().Trim("'").Trim('"')
[Environment]::SetEnvironmentVariable('CURSOR_API_KEY',$key,'User')
```

## Cursor 全局 MCP

已可在 `%USERPROFILE%\.cursor\mcp.json` 登记 `smartgis-debug`（`python -m debug.mcp`）。  
改完后 **Reload Window**；主对话与扩展 Chat 都可走同一 MCP。

## 测试

```bat
set PYTHONPATH=tools
py -3 -m pytest tools\debug\tests\test_mcp_tools.py -q
```
