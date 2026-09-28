---
name: auto-commit-push
description: >-
  Use when the user asks to auto commit and push, invoke /auto-commit-push,
  or says 自动提交并push. Commits on master and pushes to the tracking remote.
  Invoking this skill authorizes commit + push for the current turn.
---

<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# Auto commit and push

**自动提交并 push**

Invoking this skill **authorizes** `git commit` and `git push` for this turn.

## Hard rules

1. Stay on **`master`**. Do not create or switch branches.
2. NEVER update git config. NEVER force-push. NEVER `--no-verify` / `--no-gpg-sign`.
3. NEVER add Cursor `Co-authored-by` trailers (`cursoragent@cursor.com` / “Cursor Agent”).
4. Skip empty commits (no staged/unstaged/untracked changes worth committing).
5. Warn and **exclude** secrets (`.env`, `credentials.json`, keys, tokens). Do not commit them.

## Steps (normative)

1. In parallel from repo root:
   - `git status`
   - `git diff` and `git diff --staged`
   - `git log -5 --oneline` (match message style)
2. If nothing to commit: report in **简体中文** and stop (no empty commit, no push).
3. Draft a concise 1–2 sentence message focused on **why**.
4. Stage relevant files (`git add`). Exclude secrets and unrelated junk (e.g. `.tmp/` noise unless asked).
5. Commit with a PowerShell-compatible here-string:

```powershell
$msg = @'
Commit message here.

'@
git commit -m $msg
```

6. Verify: `git status` and `git log -1 --format=%B` — if a Cursor co-author trailer appears, rewrite the message (new commit or amend only when amend rules allow) so it is clean.
7. `git push` to the tracking remote (not force). If no upstream, report and stop; do not invent remotes.
8. Report result in **简体中文** (commit subject, push ok/fail).

## Communication

- Progress in **简体中文**
- Code / identifiers in **English**
