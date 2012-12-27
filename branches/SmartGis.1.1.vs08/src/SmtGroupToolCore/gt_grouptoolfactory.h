/*
File:   gt_grouptoolfactory.h 

Desc:   工具创建工厂

Version: Version 1.0

Writter:  陈春亮

Date:    2010.11.17

Copyright (c) 2010 CCL. All rights reserved.
*/
#ifndef _GT_GROUPTOOLFACTORY_H
#define _GT_GROUPTOOLFACTORY_H

#include "gt_basetool.h"
#include "gt_base3dtool.h"
#include "gt_defs.h"

using namespace Smt_GroupTool;

enum GroupToolType
{
	GTT_InputPoint,
	GTT_InputLine,
	GTT_InputRegion,
	GTT_AppendFeature,
	GTT_ViewControl,
	GTT_Flash,
	GTT_Select,
	GTT_WSViewControl,
};

enum GroupTool3DType
{
	GTT_3DViewControl,
};


class SMT_EXPORT_CLASS SmtGroupToolFactory
{
public:
	//create
	static int               CreateGroupTool(SmtBaseTool* & pTool,GroupToolType type);
	static int               CreateGroup3DTool(SmtBase3DTool* & pTool,GroupTool3DType type);

	//destroy
	static int               DestoryGroupTool(SmtBaseTool* & pTool);
	static int               DestoryGroup3DTool(SmtBase3DTool* & pTool);

private:
	SmtGroupToolFactory(void);
	virtual ~SmtGroupToolFactory(void);
};

#if !defined(Export_SmtGroupToolCore)
#if   defined( _DEBUG)
#          pragma comment(lib,"SmtGroupToolCoreD.lib")
#       else
#          pragma comment(lib,"SmtGroupToolCore.lib")
#	    endif  
#endif

#endif //_GT_GROUPTOOLFACTORY_H