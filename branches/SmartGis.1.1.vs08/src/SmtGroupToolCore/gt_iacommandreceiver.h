/*
File:    gt_iacommandreceiver.h

Desc:    SmtToolCommandReceiver,��������������Ӧ

Version: Version 1.0

Writter:  �´���

Date:    2010.11.17

Copyright (c) 2010 CCL. All rights reserved.
*/
#ifndef _GT_TOOLCOMMANDRECEIVER_H
#define _GT_TOOLCOMMANDRECEIVER_H

#include "gt_basetool.h"
#include "smt_command.h"
using namespace  Smt_Core;

namespace Smt_GroupTool
{
	class SmtIACommandReceiver:public SmtCommandReceiver
	{
	public:
		SmtIACommandReceiver(LPRENDERDEVICE	pRenderDevice,SmtMap *pOperMap,SmtFeature *pFeature,SmtIAType iaType);
		virtual ~SmtIACommandReceiver();

	public:
		virtual bool		Action(bool bUndo);

	public:
		SmtIAType			m_iaType;
		SmtFeature			*m_pFeature;
		SmtMap				*m_pOperMap;
		LPRENDERDEVICE		m_pRenderDevice;
	};
}

#endif //_GT_TOOLCOMMANDRECEIVER_H
