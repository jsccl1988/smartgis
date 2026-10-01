// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.
// Product plugin print.preview sample+viz (modals suppressed).

script "plugin.print" {
  seq @inproc {
    suppress_dialogs(on=1);
    load_china_sample(stub=1);
    mark("china-ok");
    fit_extent();
    pump(ms=200);
    require_plugins();
    mark("print-dialog-ok");
    export_bmp(leaf="plugin-showcase-print.bmp", frame="china_product");
    mark("bmp-ok");
    mark("pass");
    detach_maps();
  }
}
