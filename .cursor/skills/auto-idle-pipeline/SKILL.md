---
name: auto-idle-pipeline
description: >-
  Primary entrypoint for the idle finish pipeline. Use when the user invokes
  /auto-idle-pipeline, or says 空闲流水线, 对话都完成后, 每小时巡检收尾, or asks
  to run the four-stage sequence auto-build-fix → auto-bug-fix →
  auto-cbm-gen → auto-commit-push after other project conversations are idle.
---

<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# Auto idle pipeline (umbrella)

**空闲时一键收尾：编译修复检查 → e2e/te → CBM 索引 → 提交并 push**

This is the **top-level** project skill to call directly (`/auto-idle-pipeline`).
Component skills remain separately callable: `auto-build-fix`, `auto-bug-fix`,
`auto-cbm-gen`, `auto-commit-push`.

## When to run stages

1. **Idle check** (default): other agent conversations for this project must be idle/complete.
2. **Force**: if the user says `force` / `跳过闲置检查`, skip the idle check and run stages.

## Idle detection

Paths:

- Transcripts: `C:/Users/LENOVO/.cursor/projects/c-Dev-src-gis-smartgis/agent-transcripts` (uuid folders / `*.jsonl`)
- Terminals: `C:/Users/LENOVO/.cursor/projects/c-Dev-src-gis-smartgis/terminals/*.txt`

Heuristic — prefer **false-negative** (skip pipeline) over colliding with active agents:

- A conversation is **active** if its transcript was modified within the last **~10 minutes** AND the latest entries look like an in-progress agent turn (not a settled user-wait).
- Exclude **this** conversation (the one running the pipeline) from the active set.
- Long-running agent commands in terminals metadata (status running, recent `running_for_ms`) count as active work.

If any other conversation is active: report **跳过** in **简体中文** and stop.

## Stage order (normative)

When idle (or forced), **read and follow** each skill’s `SKILL.md` in order:

1. **`.cursor/skills/auto-build-fix/SKILL.md`**
   - MUST run `.\build.bat` and fix until green (done bar in that skill).
   - Continue to the next stage only after a green compile (or hard-stop documented in that skill).
2. **`.cursor/skills/auto-bug-fix/SKILL.md`**
   - MUST run `.\build.bat e2e` then `.\build.bat te` and fix until green (done bar in that skill).
   - Allowed by `.cursor/rules/build/agent-may-build.mdc`.
3. **`.cursor/skills/auto-cbm-gen/SKILL.md`** — CBM full index update.
4. **`.cursor/skills/auto-commit-push/SKILL.md`** — commit on `master` and push (skill authorizes).

Stay on **`master`**. No new branches. No Cursor Automation (`open_automation`); local loop only.

## Arming the 1h loop (local IDE)

Use the Loop skill **Monitored shell output** pattern. PowerShell:

```powershell
while ($true) {
  Start-Sleep -Seconds 3600
  Write-Output 'AGENT_LOOP_TICK_idle_pipeline {"prompt":"/auto-idle-pipeline"}'
}
```

1. Check terminals for an existing matching loop; do not duplicate.
2. Start background Shell with `block_until_ms: 0` and `notify_on_output` pattern `^AGENT_LOOP_TICK_idle_pipeline` (reason e.g. `idle pipeline tick`).
3. Run `/auto-idle-pipeline` logic **once immediately** after arming; first sleep tick is 1h later.
4. Confirm interval, that the first check ran, and PID if known.

On each later tick: re-read this skill and run the umbrella again.

## Communication

- Progress in **简体中文**
- Code / identifiers in **English**
