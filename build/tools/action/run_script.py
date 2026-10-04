#!/usr/bin/env python
# Copyright (c) 2026 The Mogu Authors.
# All rights reserved.
# -*- coding: utf-8 -*-
"""GN action helper: run a command or script (mogu build/tools/action/run_script.py)."""

from __future__ import print_function

import os
import shutil
import stat
import subprocess
import sys


def _find_java():
    env = os.environ.get("JAVA_HOME", "").strip()
    if env:
        for name in ("java.exe", "java"):
            cand = os.path.join(env, "bin", name)
            if os.path.isfile(cand):
                return cand
    which = shutil.which("java")
    if which:
        return which
    repo = os.path.abspath(
        os.path.join(os.path.dirname(__file__), os.pardir, os.pardir, os.pardir)
    )
    jdk = os.path.join(repo, "third_party", ".tools", "jdk21")
    roots = [
        jdk,
        r"C:\Program Files\Microsoft",
        r"C:\Program Files\Eclipse Adoptium",
        r"C:\Program Files\Java",
    ]
    for root in roots:
        if not os.path.isdir(root):
            continue
        for dirpath, _, files in os.walk(root):
            if "java.exe" in files:
                return os.path.join(dirpath, "java.exe")
            if "java" in files:
                return os.path.join(dirpath, "java")
    return "java"


def run_command(cmd, args):
    if cmd == "java":
        cmd = _find_java()
    try:
        process = subprocess.Popen(
            [cmd] + args,
            stdout=sys.stdout,
            stderr=sys.stderr,
            universal_newlines=True,
        )
        process.wait()
        if process.returncode != 0:
            sys.exit(process.returncode)
    except OSError as e:
        sys.stderr.write("Error running command '{}': {}\n".format(cmd, e))
        sys.exit(1)


def run_script(script_path, script_args):
    if not os.path.isabs(script_path):
        script_path = os.path.abspath(script_path)
    if not os.path.exists(script_path):
        sys.stderr.write("Error: Script not found: {}\n".format(script_path))
        sys.exit(1)
    if os.name != "nt":
        try:
            st = os.stat(script_path)
            os.chmod(script_path, st.st_mode | stat.S_IEXEC | stat.S_IXGRP | stat.S_IXOTH)
        except OSError as e:
            sys.stderr.write("Warning: Could not make script executable: {}\n".format(e))
    try:
        if os.name == "nt":
            shell_cmd = ["bash", script_path] + script_args
        else:
            shell_cmd = [script_path] + script_args
        process = subprocess.Popen(
            shell_cmd,
            stdout=sys.stdout,
            stderr=sys.stderr,
            universal_newlines=True,
        )
        process.wait()
        if process.returncode != 0:
            sys.exit(process.returncode)
    except OSError as e:
        if os.name == "nt":
            sys.stderr.write("Error: bash not found. Please install WSL or Git Bash.\n")
        else:
            sys.stderr.write("Error: Could not execute script: {}\n".format(e))
        sys.exit(1)


def main():
    if len(sys.argv) < 2:
        sys.stderr.write(
            "Usage: run_script.py <script_path_or_command> [args...]\n"
        )
        sys.exit(1)
    target = sys.argv[1]
    args = sys.argv[2:] if len(sys.argv) > 2 else []
    abs_target = target if os.path.isabs(target) else os.path.abspath(target)
    if os.path.isfile(abs_target):
        run_script(abs_target, args)
    else:
        run_command(target, args)


if __name__ == "__main__":
    main()
