# Copyright (c) 2026 The Mogu Authors.
# All rights reserved.
# Plugins dock only. No Report / open_report side effects.
# Tab index matches shell_layout_composer: Tools=0 … Atmosphere=11 Plugins=12.

import glang.plugin as gp

def main():
    gp.mark("show")
    gp.pump(ms=400)
    gp.require_hwnd()
    gp.mark("hwnd-ok")
    gp.suppress_dialogs(on=1)
    gp.require_plugins()
    gp.mark("plugins-ok")
    gp.inspector_tab(index=12)
    gp.pump(ms=400)
    gp.mark("plugins-tab-ok")
    gp.mark("pass")
