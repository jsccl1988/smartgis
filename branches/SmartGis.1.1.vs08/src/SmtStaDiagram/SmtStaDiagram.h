// SmtStaDiagram.h : SmtStaDiagram DLL ����ͷ�ļ�
//

#pragma once

#ifndef __AFXWIN_H__
	#error "�ڰ������ļ�֮ǰ������stdafx.h�������� PCH �ļ�"
#endif

#include "resource.h"		// ������


// CSmtStaDiagramApp
// �йش���ʵ�ֵ���Ϣ������� SmtStaDiagram.cpp
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
