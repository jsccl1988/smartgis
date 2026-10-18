# Copyright (c) 2026 The Mogu Authors.
# All rights reserved.
# Gesture body for ui.interact.combo: smoke (activate + Escape) then
# path / chord / bursts. Same host chrome as ui.interact / .os.
# Launch policy stays ContentGdi (not the FlyCube ui.interact row).

import glang.plugin as gp

def main():
    gp.window("activate")
    gp.pump(40)
    gp.key("ESCAPE")
    for _sg_i in range(2):
        gp.pump(20)
    gp.mark("interact-smoke-ok")
    gp.pump(80)
    gp.path("shell_client", (240, 300), (280, 320), (320, 310), (360, 330))
    gp.pump(40)
    with gp.chord("CTRL"):
        gp.click("shell_client", 480, 360)
    gp.pump(40)
    gp.pan_burst("shell_client", count=3, x=260, y=300, dx=28, dy=16, pump_ms=25)
    gp.wheel_burst("shell_client", count=2, x=400, y=400, delta=-120, pump_ms=30)
    gp.key("ESCAPE")
    gp.mark("interact-combo-demo-ok")
    gp.mark("pass")
