// xcatalog_core.h : SmtXCatalogCore DLL ����ͷ�ļ�
//

#pragma once

#ifndef __AFXWIN_H__
	#error "�ڰ������ļ�֮ǰ������stdafx.h�������� PCH �ļ�"
#endif

#include "legacy_ui/xcatalog/resource.h"		// ������


// CSmtXCatalogCoreApp
// �йش���ʵ�ֵ���Ϣ������� xcatalog_core.cpp
//

class CSmtXCatalogCoreApp : public CWinApp
{
public:
	CSmtXCatalogCoreApp();

// ��д
public:
	virtual BOOL InitInstance();

	DECLARE_MESSAGE_MAP()
};
