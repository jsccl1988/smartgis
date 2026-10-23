# Copyright (c) 2026 The Mogu Authors.
# All rights reserved.

"""Sample kind=python pack: core verbs via glang.plugin, own surface via glang.ext."""

import glang.plugin as gp
import glang.ext.smartgis.sample_hello_ext as ext


def start(host):
    if gp is None:
        raise RuntimeError("glang.plugin missing")
    if ext is None or not hasattr(ext, "ping"):
        raise RuntimeError("glang.ext.smartgis.sample_hello_ext missing")

    def _run(args):
        # Core mark (when host bound) is optional in unit tests; ping is local.
        _ = ext.ping()
        return True

    host.contribute_command(
        "smartgis.sample_hello_ext",
        "sample.hello_ext",
        "Hello Ext",
        "tools",
        _run,
    )


def stop():
    pass
