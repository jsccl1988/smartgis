// view_core.h : SmtViewCore DLL ����ͷ�ļ�
//

#pragma once

#ifndef __AFXWIN_H__
	#error "�ڰ������ļ�֮ǰ������stdafx.h�������� PCH �ļ�"
#endif

#include "ui/xview/resource.h"		// ������


// CSmtViewCoreApp
// �йش���ʵ�ֵ���Ϣ������� view_core.cpp
//

class CSmtViewCoreApp : public CWinApp
{
public:
	CSmtViewCoreApp();

// ��д
public:
	virtual BOOL InitInstance();

	DECLARE_MESSAGE_MAP()
};
