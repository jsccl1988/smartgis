// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "legacy/render/bridge/leftover_record.h"

// Process-wide leftover RHI session. GDI / GDI-simple / GL resolve this
// export so they share one Device + CommandList.

extern "C" __declspec(dllexport) render::scene::LeftoverRecorder*
smt_leftover_session() {
  static render::scene::LeftoverRecorder session;
  return &session;
}
