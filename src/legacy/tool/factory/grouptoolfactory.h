// Copyright (c) 2010 CCL. All rights reserved.
#ifndef _GT_GROUPTOOLFACTORY_H
#define _GT_GROUPTOOLFACTORY_H

#include "legacy/tool/base/base3dtool.h"
#include "legacy/tool/base/basetool.h"
#include "legacy/tool/defs.h"
#include "legacy/ui/ui_legacy_export.h"

using namespace tool;

enum GroupToolType {
  GTT_InputPoint,
  GTT_InputLine,
  GTT_InputRegion,
  GTT_AppendFeature,
  GTT_ViewControl,
  GTT_Flash,
  GTT_Select,
};

enum GroupTool3DType {
  GTT_3DViewControl,
};

// Static factory for leftover group tools (2D / 3D). Keeps plugin ABI.
class UI_LEGACY_EXPORT SmtGroupToolFactory {
 public:
  SmtGroupToolFactory() = delete;
  ~SmtGroupToolFactory() = delete;

  static int CreateGroupTool(SmtBaseTool*& pTool, GroupToolType type);
  static int CreateGroup3DTool(SmtBase3DTool*& pTool, GroupTool3DType type);

  // Spelling Destory* is leftover ABI - do not rename.
  static int DestoryGroupTool(SmtBaseTool*& pTool);
  static int DestoryGroup3DTool(SmtBase3DTool*& pTool);
};

#endif  //_GT_GROUPTOOLFACTORY_H
