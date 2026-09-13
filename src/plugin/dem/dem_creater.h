// am_dem_creater.h : SmtAMDemCreater DLL ����ͷ�ļ�
//

#pragma once

#ifndef __AFXWIN_H__
	#error "�ڰ������ļ�֮ǰ������stdafx.h�������� PCH �ļ�"
#endif

#include "plugin/dem/resource.h"		// ������


// CSmtAMDemCreaterApp
// �йش���ʵ�ֵ���Ϣ������� am_dem_creater.cpp
//

class CSmtAMDemCreaterApp : public CWinApp
{
public:
	CSmtAMDemCreaterApp();

// ��д
public:
	virtual BOOL InitInstance();

	DECLARE_MESSAGE_MAP()
};
