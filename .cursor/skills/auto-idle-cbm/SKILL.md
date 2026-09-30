---
name: auto-idle-cbm
description: >-
  Lightweight idle entry that refreshes only the CBM index. Use when the user
  invokes /auto-idle-cbm, or says 闲置更新CBM, 空闲时更新索引, 闲置时更新CBM,
  or asks to update the codebase-memory index when other conversations are idle.
  Arms an optional 1h Loop with sentinel AGENT_LOOP_TICK_idle_cbm. Does not run
  build, e2e, or commit — only auto-cbm-gen after the idle gate.
---

<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# Auto idle CBM (lightweight)

**闲置时只更新 CBM 索引**（不跑编译 / e2e / 提交）

Lightweight companion to `/auto-idle-pipeline`. Same idle gate; after idle (or
force), **only** read and follow `.cursor/skills/auto-cbm-gen/SKILL.md`.

## Preconditions

1. **Idle check** (default): other agent conversations for this project must be idle/complete before indexing.
2. **Force**: if the user says `force` / `跳过闲置检查` / `强制收尾`, skip the idle check and run `auto-cbm-gen`.
3. Work only on **`master`**. No new branches. No Cursor Automation (`open_automation`); local IDE + Loop only.
4. Do **not** run build, e2e/te, commit, or push from this skill.

## Idle detection

Same heuristic as `/auto-idle-pipeline` — prefer **false-negative** (skip) over colliding with active agents.

Paths:

- Transcripts: `C:/Users/LENOVO/.cursor/projects/c-Dev-src-gis-smartgis/agent-transcripts` (uuid folders / `*.jsonl`)
- Terminals: `C:/Users/LENOVO/.cursor/projects/c-Dev-src-gis-smartgis/terminals/*.txt`

Rules:

- A conversation is **active** if its transcript was modified within the last **~10 minutes** AND the latest entries look like an in-progress agent turn (not a settled user-wait / finished reply).
- Exclude **this** conversation (the one running idle-cbm) from the active set.
- Long-running agent commands in terminals metadata (`running_for_ms`, active command) count as active work — except known idle Loop shells (`AGENT_LOOP_TICK_idle_cbm` / `AGENT_LOOP_TICK_idle_pipeline`).
- If heuristics are ambiguous, **skip** and report why.

If any other conversation is active: report **跳过（其他对话仍在进行）** in **简体中文** and **stop**. This is a successful “checked and deferred,” not a failure.

## After idle (or forced)

1. Open `.cursor/skills/auto-cbm-gen/SKILL.md` and follow it fully (authorization, steps, done bar, hard stops).
2. Do **not** invoke `auto-build-fix`, `auto-bug-fix`, or `auto-commit-push`.

CBM target (from `auto-cbm-gen`): MCP `user-codebase-memory-mcp`, project `smartgis`, root `C:/Dev/src/gis/smartgis`.

## Done bar

1. Idle check passed **or** user forced skip.
2. `auto-cbm-gen` done bar met (or hard-stop reported — then this skill is **not** done).

## Arming the 1h loop (local IDE)

Use the Loop skill **Monitored shell output** pattern. PowerShell:

```powershell
while ($true) {
  Start-Sleep -Seconds 3600
  Write-Output 'AGENT_LOOP_TICK_idle_cbm {"prompt":"/auto-idle-cbm"}'
}
```

1. Check terminals for an existing matching loop (`AGENT_LOOP_TICK_idle_cbm`); do not duplicate.
2. Start background Shell with `block_until_ms: 0` and `notify_on_output` pattern `^AGENT_LOOP_TICK_idle_cbm` (reason e.g. `idle cbm tick`).
3. Run `/auto-idle-cbm` logic **once immediately** after arming; first sleep tick is 1h later.
4. Confirm interval, that the first check ran, and PID if known.

**Coexistence:** sentinel differs from `AGENT_LOOP_TICK_idle_pipeline`. Both loops may run together. Do **not** kill the pipeline loop when arming this one.

Do **not** use `open_automation` / Cursor Automations for this task.

## Hard constraints

- Stay on **`master`**. No feature / `cursor/*` branches.
- No `git config` changes; no commit/push from this skill.
- No product C++ / build changes just to refresh the graph.
- Progress in **简体中文**; code / paths / identifiers in **English**.

## Relation

| Call | Role |
|------|------|
| `/auto-idle-cbm` | Idle gate + **CBM-only** + optional 1h Loop (`AGENT_LOOP_TICK_idle_cbm`) |
| `/auto-idle-pipeline` | Umbrella: idle gate + build → e2e → CBM → commit (separate Loop) |
| `/auto-cbm-gen` | CBM index only (no idle gate) |

When this skill runs the index stage, `auto-cbm-gen` authorization and done bar apply as if the user had invoked that skill directly.
