// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef UI_VIEWS_MAP_VIEWPORT_FEATURES_H_
#define UI_VIEWS_MAP_VIEWPORT_FEATURES_H_

// Optional product includes for DrawHost translation units. Keep the
// __has_include probes in one place so sibling .cc files stay in sync.

#if defined(__has_include)
#if __has_include("content/public/map_contents.h")
#include "content/public/map_contents.h"
#define HAS_CONTENT_MAP_SESSION 1
#endif
#if __has_include("content/public/view_host.h")
#include "content/public/view_host.h"
#define HAS_VIEW_HOST 1
#endif
#if __has_include("content/browser/present/scene3d/session/scene3d_rhi_session.h")
#include "content/browser/present/scene3d/session/scene3d_rhi_session.h"
#define HAS_SCENE3D_ENGINE 1
#endif
#if __has_include("ui/shell/map_session.h")
#include "ui/shell/map_session.h"
#define HAS_UI_SHELL 1
#endif
#endif

#endif  // UI_VIEWS_MAP_VIEWPORT_FEATURES_H_
