// SmtXCatalogCore.h : SmtXCatalogCore DLL ����ͷ�ļ�
//

#pragma once

#ifndef __AFXWIN_H__
	#error "�ڰ������ļ�֮ǰ������stdafx.h�������� PCH �ļ�"
#endif

#include "resource.h"		// ������


// CSmtXCatalogCoreApp
// �йش���ʵ�ֵ���Ϣ������� SmtXCatalogCore.cpp
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
