# Copyright (c) 2026 The Mogu Authors.
# All rights reserved.
# Resolve china_city.gpkg. Callers decide fit_extent and waits.

from steps.support import fail_kwargs


def resolve_china_city(sg):
    china = sg.resolve_data(kind="data", leaf="china_city.gpkg")
    if not china:
        sg.mark("china-seed-miss")
        raise RuntimeError("china seed missing")
    return china


def open_china_city(sg, fail_rc=None):
    china = resolve_china_city(sg)
    sg.open_map(path=china, **fail_kwargs(fail_rc))
