/*
File:   gt_grouptoolfactory.h

Desc:   ���ߴ�������

Version: Version 1.0

Writter:  �´���

Date:    2010.11.17

Copyright (c) 2010 CCL. All rights reserved.
*/
#ifndef _GT_GROUPTOOLFACTORY_H
#define _GT_GROUPTOOLFACTORY_H

#include "legacy/tool/group/base3dtool.h"
#include "legacy/tool/group/basetool.h"
#include "legacy/tool/group/defs.h"
#include "legacy/tool/group/tool_group_export.h"

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

class TOOL_GROUP_EXPORT SmtGroupToolFactory {
 public:
  // create
  static int CreateGroupTool(SmtBaseTool*& pTool, GroupToolType type);
  static int CreateGroup3DTool(SmtBase3DTool*& pTool, GroupTool3DType type);

  // destroy
  static int DestoryGroupTool(SmtBaseTool*& pTool);
  static int DestoryGroup3DTool(SmtBase3DTool*& pTool);

 private:
  SmtGroupToolFactory(void);
  virtual ~SmtGroupToolFactory(void);
};

#endif  //_GT_GROUPTOOLFACTORY_H