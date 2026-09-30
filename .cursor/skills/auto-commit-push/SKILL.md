---
name: auto-commit-push
description: >-
  Auto commit on master and push to the tracking remote. Use when the user
  invokes /auto-commit-push, or says 自动提交并push, or when auto-idle-pipeline
  reaches the commit stage. Invoking this skill authorizes commit + push for
  the current turn. Never sets git config; stops if user.name/email is missing.
---

<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# Auto commit and push

**自动提交并 push**

Independently callable (`/auto-commit-push`) and also stage 4 of `/auto-idle-pipeline`.

Invoking this skill **authorizes** `git commit` and `git push` for this turn only.

## Hard rules

1. Stay on **`master`**. Do not create or switch branches.
2. NEVER update git config (`user.name`, `user.email`, or any other). NEVER force-push. NEVER `--no-verify` / `--no-gpg-sign`.
3. NEVER add Cursor `Co-authored-by` trailers (`cursoragent@cursor.com` / “Cursor Agent”).
4. Skip empty commits (no staged/unstaged/untracked changes worth committing).
5. Warn and **exclude** secrets (`.env`, `credentials.json`, keys, tokens). Do not commit them.

## Identity gate (before commit)

```powershell
git config user.name
git config user.email
```

If either is empty / unset: **stop**. Report in **简体中文** that commit identity is missing and the user must set it locally. Do **not** invent values; do **not** run `git config` to fix it.

## Steps (normative)

1. Confirm branch is `master` (`git status` / `git branch --show-current`). If not: **stop** and report.
2. Identity gate (above).
3. In parallel from repo root:
   - `git status`
   - `git diff` and `git diff --staged`
   - `git log -5 --oneline` (match message style)
4. If nothing to commit: report in **简体中文** and stop (no empty commit, no push). This is a clean skip, not a failure.
5. Draft a concise 1–2 sentence message focused on **why**.
6. Stage relevant files (`git add`). Exclude secrets and unrelated junk (e.g. `.tmp/` noise unless asked).
7. Commit with a PowerShell-compatible here-string:

```powershell
$msg = @'
Commit message here.

'@
git commit -m $msg
```

8. Verify: `git status` and `git log -1 --format=%B` — if a Cursor co-author trailer appears, rewrite so it is clean (new commit; amend only when amend rules allow).
9. `git push` to the tracking remote (not force). If no upstream: report and **stop**; do not invent remotes.
10. Report result in **简体中文** (commit subject, push ok/fail).

## Done bar

- Clean skip (nothing to commit), **or**
- Commit created on `master` without Cursor co-author trailer, **and** `git push` succeeded.

## Hard stops

- Not on `master`
- Missing `user.name` / `user.email`
- Secrets in the would-be commit set (exclude them; if only secrets remain, stop)
- No upstream / push rejected
- Hooks reject the commit — fix if trivial and create a **new** commit; do not `--no-verify`

## Communication

- Progress in **简体中文**
- Code / identifiers in **English**
