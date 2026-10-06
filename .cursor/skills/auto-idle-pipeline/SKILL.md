---
name: auto-idle-pipeline
description: >-
  Primary entrypoint for the idle finish pipeline. Use when the user invokes
  /auto-idle-pipeline, or says 空闲流水线, 对话都完成后, 每小时巡检收尾, 四阶段一键收尾,
  or asks to run the four-stage sequence auto-build-fix → auto-bug-fix →
  auto-cbm-gen → auto-commit-push after other project conversations are idle.
  Arms an optional 1h Loop with sentinel AGENT_LOOP_TICK_idle_pipeline.
---

<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# Auto idle pipeline (umbrella)

**空闲时一键收尾：编译修复 → e2e/te → CBM 索引 → 提交并 push**

This is the **preferred one-click entry** (`/auto-idle-pipeline`). The four stage
skills remain **separately** callable via `/auto-build-fix`, `/auto-bug-fix`,
`/auto-cbm-gen`, `/auto-commit-push` (or `@` attach). Do not duplicate their
internals here — **read and follow** each stage’s `SKILL.md`.

Build / e2e / te are allowed when this umbrella (or a stage skill) is invoked —
see `.cursor/rules/build/agent-may-build.mdc`.

## Preconditions

1. **Idle check** (default): other agent conversations for this project must be idle/complete before stages run.
2. **Force**: if the user says `force` / `跳过闲置检查` / `强制收尾`, skip the idle check and run stages.
3. Work only on **`master`**. No new branches. No Cursor Automation (`open_automation`); local IDE + Loop only.
4. NEVER invent or set `git user.name` / `user.email` (or any git config). Stage 4 stops if identity is missing.

## Idle detection

Paths:

- Transcripts: `C:/Users/LENOVO/.cursor/projects/c-Dev-src-gis-smartgis/agent-transcripts` (uuid folders / `*.jsonl`)
- Terminals: `C:/Users/LENOVO/.cursor/projects/c-Dev-src-gis-smartgis/terminals/*.txt`

Heuristic — prefer **false-negative** (skip pipeline) over colliding with active agents:

- A conversation is **active** if its transcript was modified within the last **~10 minutes** AND the latest entries look like an in-progress agent turn (not a settled user-wait / finished reply).
- Exclude **this** conversation (the one running the pipeline) from the active set.
- Long-running agent commands in terminals metadata (`running_for_ms`, active command) count as active work.
- If heuristics are ambiguous, **skip** and report why.

If any other conversation is active: report **跳过（其他对话仍在进行）** in **简体中文** and **stop** (do not start stages). This is a successful “checked and deferred,” not a pipeline failure.

## Stage order (normative)

When idle (or forced), run **in order**. For each stage: open that skill’s `SKILL.md` and follow it fully (done bar, hard stops, evidence rules).

| # | Skill | Path | Must achieve |
|---|-------|------|--------------|
| 1 | `auto-build-fix` | `.cursor/skills/auto-build-fix/SKILL.md` | Green compile via `.\build.bat` (done bar there) |
| 2 | `auto-bug-fix` | `.cursor/skills/auto-bug-fix/SKILL.md` | `.\build.bat debug harness` **and** `.\build.bat debug te` green |
| 3 | `auto-cbm-gen` | `.cursor/skills/auto-cbm-gen/SKILL.md` | CBM full index for project `smartgis` |
| 4 | `auto-commit-push` | `.cursor/skills/auto-commit-push/SKILL.md` | Commit on `master` + push (skill authorizes) |

**Continue to the next stage only after the current stage’s done bar** (or an explicit skip below).

### Skips (allowed)

| Situation | Action |
|-----------|--------|
| Idle check fails (not forced) | Skip **all** stages; report 跳过 |
| Stage 4: working tree clean | Skip commit/push; report 无变更可提交 — pipeline still **done** if 1–3 passed |
| Stage 3: CBM MCP unavailable / auth blocked | Report failure; **stop** (do not commit a half-finished index story as success) |
| User asks only to arm the Loop | Arm Loop; optional one immediate run if they also want a check now |

### Failures (stop the pipeline)

| Stage fails | Stop? | Next stages |
|-------------|-------|-------------|
| 1 `auto-build-fix` hard-stop or not green | **Yes** | Do **not** run 2–4 |
| 2 `auto-bug-fix` hard-stop or not green | **Yes** | Do **not** run 3–4 |
| 3 `auto-cbm-gen` fails | **Yes** | Do **not** run 4 |
| 4 `auto-commit-push` fails (no identity, push rejected, secrets) | **Yes** | Report; do not force-push or set git config |

On stop: report **哪一阶段失败、原因、是否已部分完成** in **简体中文**. Do not silently continue.

## Pipeline done bar

All of:

1. Idle check passed **or** user forced skip.
2. Stages 1–2 green per their skills (evidence: commands + exit 0 + logs).
3. Stage 3 index completed (or hard-stop reported — then pipeline **not** done).
4. Stage 4: pushed **or** cleanly skipped (nothing to commit) **or** stopped with identity/push error reported.

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

On each later tick: re-read **this** skill and run the umbrella again (idle check → stages).

Do **not** use `open_automation` / Cursor Automations for this pipeline.

## Hard constraints

- Stay on **`master`**. No feature / `cursor/*` branches.
- No `git config` changes; no invented committer identity.
- No force-push; no `--no-verify` / `--no-gpg-sign`.
- No Cursor `Co-authored-by` trailers.
- Do not open PRs unless the user separately asks.
- Progress in **简体中文**; code / paths / identifiers in **English**.

## Relation to stage skills

| Call | Role |
|------|------|
| `/auto-idle-pipeline` | Umbrella: idle gate + ordered 1→4 + optional 1h Loop |
| `/auto-idle-cbm` | CBM-only idle entry (same idle gate; no build/e2e/commit); Loop sentinel `AGENT_LOOP_TICK_idle_cbm` — may coexist with this umbrella |
| `/auto-build-fix` | Compile-only loop |
| `/auto-bug-fix` | e2e + te fix loop |
| `/auto-cbm-gen` | CBM index only |
| `/auto-commit-push` | Commit + push only (authorizes git write) |

When the umbrella runs a stage, that stage’s authorization and done bar apply as if the user had invoked that skill directly.
