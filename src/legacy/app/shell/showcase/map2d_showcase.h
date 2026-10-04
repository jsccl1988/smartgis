// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef LEGACY_APP_SHELL_MAP2D_SHOWCASE_H_
#define LEGACY_APP_SHELL_MAP2D_SHOWCASE_H_

namespace app {
class SmtApp;
}

namespace legacy_app {

// Headless GDI china paint + BMP sidecar for SmartGis.exe --map2d-showcase.
// Mirrors Views map2d_showcase: DelayInit map → ZoomToRect → SaveImage → exit.
// Writes out/Debug/legacy-map2d-showcase-china.bmp next to the exe.
// |app| is the CSmartGisApp (inherits SmtApp) that already ran Init().
// Returns process exit code (0 = PASS). Caller should TerminateProcess.
int run_map2d_showcase_china(app::SmtApp &app);

} // namespace legacy_app

#endif // LEGACY_APP_SHELL_MAP2D_SHOWCASE_H_
