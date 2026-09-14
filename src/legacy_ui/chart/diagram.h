// sta_diagram.h : SmtStaDiagram DLL ����ͷ�ļ�
//

#pragma once

#ifndef __AFXWIN_H__
	#error "�ڰ������ļ�֮ǰ������stdafx.h�������� PCH �ļ�"
#endif

#include "legacy_ui/chart/resource.h"		// ������


// CSmtStaDiagramApp
// �йش���ʵ�ֵ���Ϣ������� sta_diagram.cpp
//

class CSmtStaDiagramApp : public CWinApp
{
public:
	CSmtStaDiagramApp();

// ��д
public:
	virtual BOOL InitInstance();

	DECLARE_MESSAGE_MAP()
};
