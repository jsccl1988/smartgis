#include "legacy/tool/group/factory/grouptoolfactory.h"

#include "legacy/tool/group/base/basetool.h"
#include "legacy/tool/group/input/appendfeaturetool.h"
#include "legacy/tool/group/input/inputlinetool.h"
#include "legacy/tool/group/input/inputpointtool.h"
#include "legacy/tool/group/input/inputregiontool.h"
#include "legacy/tool/group/select/flashtool.h"
#include "legacy/tool/group/select/selecttool.h"
#include "legacy/tool/group/view/3dviewctrltool.h"
#include "legacy/tool/group/view/viewctrltool.h"

SmtGroupToolFactory::SmtGroupToolFactory(void) { ; }

SmtGroupToolFactory::~SmtGroupToolFactory(void) { ; }

int SmtGroupToolFactory::CreateGroupTool(SmtBaseTool*& pTool,
                                         GroupToolType type) {
  int nRet = SMT_ERR_FAILURE;
  if (!pTool) {
    switch (type) {
      // Input* retained for GTT_* ABI (plugins). No pointer digitizing;
      // geometry via apply_draft only. Product Append uses draw.* when bound.
      case GTT_InputPoint:
        pTool = new SmtInputPointTool();
        nRet = SMT_ERR_NONE;
        break;

      case GTT_InputLine:
        pTool = new SmtInputLineTool();
        nRet = SMT_ERR_NONE;
        break;

      case GTT_InputRegion:
        pTool = new SmtInputRegionTool();
        nRet = SMT_ERR_NONE;
        break;

      case GTT_AppendFeature:
        pTool = new SmtAppendFeatureTool();
        nRet = SMT_ERR_NONE;
        break;

      case GTT_ViewControl:
        pTool = new SmtViewCtrlTool();
        nRet = SMT_ERR_NONE;
        break;

      case GTT_Select:
        pTool = new SmtSelectTool();
        nRet = SMT_ERR_NONE;
        break;
      case GTT_Flash:
        pTool = new SmtFlashTool();
        nRet = SMT_ERR_NONE;
        break;
      default:
        break;
    }
  }

  return nRet;
}

int SmtGroupToolFactory::CreateGroup3DTool(SmtBase3DTool*& pTool,
                                           GroupTool3DType type) {
  int nRet = SMT_ERR_FAILURE;
  if (!pTool) {
    switch (type) {
      case GTT_3DViewControl:
        pTool = new Smt3DViewCtrlTool();
        nRet = SMT_ERR_NONE;
        break;
      default:
        break;
    }
  }
  return nRet;
}

int SmtGroupToolFactory::DestoryGroupTool(SmtBaseTool*& pTool) {
  SMT_SAFE_DELETE(pTool);

  return SMT_ERR_NONE;
}

int SmtGroupToolFactory::DestoryGroup3DTool(SmtBase3DTool*& pTool) {
  SMT_SAFE_DELETE(pTool);

  return SMT_ERR_FAILURE;
}