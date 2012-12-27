// SmtXCatalog.cpp : 实现文件
//

#include "stdafx.h"
#include "SmtXCatalogCore.h"
#include "cata_xcatalog.h"

// SmtXCatalog

namespace Smt_XCatalog
{
	IMPLEMENT_DYNAMIC(SmtXCatalog, CTreeCtrl)

		SmtXCatalog::SmtXCatalog()
	{
		m_hContexMenu = NULL;
	}

	SmtXCatalog::~SmtXCatalog()
	{
	}


	BEGIN_MESSAGE_MAP(SmtXCatalog, CTreeCtrl)
		ON_WM_CREATE()
	END_MESSAGE_MAP()

	// SmtXCatalog 消息处理程序
	bool SmtXCatalog::InitCreate(void) 
	{ 
		return CreateContexMenu();
	}

	bool SmtXCatalog::EndDestory(void) 
	{ 
		::DestroyMenu(m_hContexMenu);

		return true;
	}

	int SmtXCatalog::OnCreate(LPCREATESTRUCT lpCreateStruct)
	{
		if (CTreeCtrl::OnCreate(lpCreateStruct) == -1)
			return -1;

		// TODO:  在此添加您专用的创建代码
		if (!InitCreate())
		{
			return -1;
		}

		return 0;
	}
}