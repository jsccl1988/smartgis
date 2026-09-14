// xambox_core.h : SmtXAMBoxCore DLL ����ͷ�ļ�
//

#pragma once

#ifndef __AFXWIN_H__
	#error "�ڰ������ļ�֮ǰ������stdafx.h�������� PCH �ļ�"
#endif

#include "legacy_ui/xambox/resource.h"		// ������


// CSmtXAMBoxCoreApp
// �йش���ʵ�ֵ���Ϣ������� xambox_core.cpp
//

class CSmtXAMBoxCoreApp : public CWinApp
{
public:
	CSmtXAMBoxCoreApp();

// ��д
public:
	virtual BOOL InitInstance();

	DECLARE_MESSAGE_MAP()
};
