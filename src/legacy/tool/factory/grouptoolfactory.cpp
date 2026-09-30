// Copyright (c) 2010 CCL. All rights reserved.

#include "legacy/tool/factory/grouptoolfactory.h"

#include <memory>
#include <utility>

#include "legacy/tool/draft/appendfeaturetool.h"
#include "legacy/tool/draft/inputlinetool.h"
#include "legacy/tool/draft/inputpointtool.h"
#include "legacy/tool/draft/inputregiontool.h"
#include "legacy/tool/select/flashtool.h"
#include "legacy/tool/select/selecttool.h"
#include "legacy/tool/nav/3dviewctrltool.h"
#include "legacy/tool/nav/viewctrltool.h"

namespace {

template <typename Base>
int create_if_null(Base*& out, Base* (*maker)()) {
  if (out != nullptr) {
    return SMT_ERR_FAILURE;
  }
  out = maker();
  return out != nullptr ? SMT_ERR_NONE : SMT_ERR_FAILURE;
}

template <typename Derived, typename Base>
Base* make() {
  return new Derived();
}

template <typename Base>
int destroy_tool(Base*& pTool) {
  std::unique_ptr<Base> owned(std::exchange(pTool, nullptr));
  return SMT_ERR_NONE;
}

}  // namespace

int SmtGroupToolFactory::CreateGroupTool(SmtBaseTool*& pTool,
                                         GroupToolType type) {
  // Input* retained for GTT_* ABI (plugins). No pointer digitizing;
  // geometry via apply_draft only. Product Append uses draw.* when bound.
  switch (type) {
    case GTT_InputPoint:
      return create_if_null(pTool, &make<SmtInputPointTool, SmtBaseTool>);
    case GTT_InputLine:
      return create_if_null(pTool, &make<SmtInputLineTool, SmtBaseTool>);
    case GTT_InputRegion:
      return create_if_null(pTool, &make<SmtInputRegionTool, SmtBaseTool>);
    case GTT_AppendFeature:
      return create_if_null(pTool, &make<SmtAppendFeatureTool, SmtBaseTool>);
    case GTT_ViewControl:
      return create_if_null(pTool, &make<SmtViewCtrlTool, SmtBaseTool>);
    case GTT_Select:
      return create_if_null(pTool, &make<SmtSelectTool, SmtBaseTool>);
    case GTT_Flash:
      return create_if_null(pTool, &make<SmtFlashTool, SmtBaseTool>);
    default:
      return SMT_ERR_FAILURE;
  }
}

int SmtGroupToolFactory::CreateGroup3DTool(SmtBase3DTool*& pTool,
                                           GroupTool3DType type) {
  switch (type) {
    case GTT_3DViewControl:
      return create_if_null(pTool, &make<Smt3DViewCtrlTool, SmtBase3DTool>);
    default:
      return SMT_ERR_FAILURE;
  }
}

int SmtGroupToolFactory::DestoryGroupTool(SmtBaseTool*& pTool) {
  return destroy_tool(pTool);
}

int SmtGroupToolFactory::DestoryGroup3DTool(SmtBase3DTool*& pTool) {
  return destroy_tool(pTool);
}
