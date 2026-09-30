// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.
#ifndef SMT_LEGACY_CORE_TYPES_H
#define SMT_LEGACY_CORE_TYPES_H

#include "legacy/core/types/point.h"
#include "legacy/core/types/rect.h"
#include "legacy/core/types/scalars.h"
#include "legacy/core/types/variant.h"

namespace base {

struct SmtTriangle {
  long a = -1;
  long b = -1;
  long c = -1;
  bool bDelete = false;

  SmtTriangle() = default;
};

typedef SmtTriangle Smt3DTriangle;

struct SmtTile {
  long lID = -1;
  long lTileBufSize = 0;
  char* pTileBuf = nullptr;
  long lImageCode = -1;
  bool bVisible = true;

  fRect rtTileRect;

  SmtTile() = default;
};

typedef SmtTile SmtWSTile;

}  // namespace base

#endif  // SMT_LEGACY_CORE_TYPES_H
