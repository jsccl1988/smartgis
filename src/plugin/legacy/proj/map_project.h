// am_map_project.h : SmtAMMapProject DLL ����ͷ�ļ�
//

#pragma once

#ifndef __AFXWIN_H__
	#error "�ڰ������ļ�֮ǰ������stdafx.h�������� PCH �ļ�"
#endif

#include "plugin/legacy/proj/resource.h"		// ������


// CSmtAMMapProjectApp
// �йش���ʵ�ֵ���Ϣ������� am_map_project.cpp
//

class CSmtAMMapProjectApp : public CWinApp
{
public:
	CSmtAMMapProjectApp();

// ��д
public:
	virtual BOOL InitInstance();

	DECLARE_MESSAGE_MAP()
};
