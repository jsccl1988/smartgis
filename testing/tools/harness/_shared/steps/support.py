# Copyright (c) 2026 The Mogu Authors.
# All rights reserved.
# Shared step boilerplate: keyword shaping, repo root, fixture text.

import os


def fail_kwargs(fail_rc):
    if fail_rc is None:
        return {}
    return {"fail_rc": fail_rc}


def repo_root():
    here = os.path.dirname(os.path.abspath(__file__))
    return os.path.abspath(os.path.join(here, "..", "..", "..", "..", ".."))


def slash(path):
    return path.replace("\\", "/")


def write_text(path, body):
    with open(path, "w", encoding="utf-8") as out:
        out.write(body)
