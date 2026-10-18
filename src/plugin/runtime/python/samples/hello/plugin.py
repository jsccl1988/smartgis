# Copyright (c) 2026 The Mogu Authors.
# All rights reserved.

import glang.plugin as gp


def start(host):
    if gp is None:
        raise RuntimeError("glang.plugin missing")

    def _run(args):
        return True

    host.contribute_command(
        "smartgis.sample_hello", "sample.hello", "Hello", "tools", _run
    )


def stop():
    pass
