#include "legacy/tool/group/grouptoolfactory.h"

#include "legacy/tool/group/3dviewctrltool.h"
#include "legacy/tool/group/appendfeaturetool.h"
#include "legacy/tool/group/basetool.h"
#include "legacy/tool/group/flashtool.h"
#include "legacy/tool/group/inputlinetool.h"
#include "legacy/tool/group/inputpointtool.h"
#include "legacy/tool/group/inputregiontool.h"
#include "legacy/tool/group/selecttool.h"
#include "legacy/tool/group/viewctrltool.h"

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

//////////////////////////////////////////////////////////////////////////
int SmtGroupToolFactory::DestoryGroupTool(SmtBaseTool*& pTool) {
  SMT_SAFE_DELETE(pTool);

  return SMT_ERR_NONE;
}

int SmtGroupToolFactory::DestoryGroup3DTool(SmtBase3DTool*& pTool) {
  SMT_SAFE_DELETE(pTool);

  return SMT_ERR_FAILURE;
}