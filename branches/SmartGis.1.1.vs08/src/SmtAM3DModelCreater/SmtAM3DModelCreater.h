// SmtAM3DModelCreater.h : SmtAM3DModelCreater DLL ����ͷ�ļ�
//

#pragma once

#ifndef __AFXWIN_H__
	#error "�ڰ������ļ�֮ǰ������stdafx.h�������� PCH �ļ�"
#endif

#include "resource.h"		// ������


// CSmtAM3DModelCreaterApp
// �йش���ʵ�ֵ���Ϣ������� SmtAM3DModelCreater.cpp
//

class CSmtAM3DModelCreaterApp : public CWinApp
{
public:
	CSmtAM3DModelCreaterApp();

// ��д
public:
	virtual BOOL InitInstance();

	DECLARE_MESSAGE_MAP()
};
