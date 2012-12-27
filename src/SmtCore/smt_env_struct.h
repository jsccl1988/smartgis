/*
File:    smt_env_struct.h

Desc:    SmartGis基础环境头文件

Version: Version 1.0

Writter:  陈春亮

Date:    2010.11.17

Copyright (c) 2010 CCL. All rights reserved.
*/
#ifndef _SMT_ENV_STRUCT_H
#define _SMT_ENV_STRUCT_H

#include "smt_core.h"

#define MAX_STYLENAME_LENGTH         MAX_NAME_LENGTH
#define MAX_MAPNAME_LENGTH           MAX_NAME_LENGTH

#define SmtPrjHead                   (SmartVersion+0x0001)

namespace Smt_Core
{
	struct SmtStyleConfig
	{//系统点、线、区默认风格
		char szPointStyle[MAX_STYLENAME_LENGTH];
		char szLineStyle[MAX_STYLENAME_LENGTH];
		char szRegionStyle[MAX_STYLENAME_LENGTH];
		char szAuxStyle[MAX_STYLENAME_LENGTH];

		char szDotFlashStyle1[MAX_STYLENAME_LENGTH];
		char szDotFlashStyle2[MAX_STYLENAME_LENGTH];
		char szLineFlashStyle1[MAX_STYLENAME_LENGTH];
		char szLineFlashStyle2[MAX_STYLENAME_LENGTH];
		char szRegionFlashStyle1[MAX_STYLENAME_LENGTH];
		char szRegionFlashStyle2[MAX_STYLENAME_LENGTH];

		SmtStyleConfig()
		{
			sprintf(szPointStyle,"DefPointStyle");
			sprintf(szLineStyle,"DefLineStyle");
			sprintf(szRegionStyle,"DefRegionStyle");
			sprintf(szAuxStyle,"DefAuxStyle");

			//flash style
			sprintf(szDotFlashStyle1,"DefAnnoFlashStyle1");
			sprintf(szDotFlashStyle2,"DefAnnoFlashStyle2");
			sprintf(szLineFlashStyle1,"DefLineFlashStyle1");
			sprintf(szLineFlashStyle2,"DefLineFlashStyle2");
			sprintf(szRegionFlashStyle1,"DefRegionFlashStyle1");
			sprintf(szRegionFlashStyle2,"DefRegionFlashStyle2");
		}
	};

	struct SmtMapDocInfo
	{//系统文档信息
		char szMapName[MAX_MAPNAME_LENGTH];
		SmtMapDocInfo()
		{
             sprintf(szMapName,"DefMap");
		}
	};

	struct SmtPrjInfo
	{//系统工程信息
		int           head;
		SmtMapDocInfo mapDocInfo;
		char          szMapDocPath[MAX_PATH];
	};

	struct SmtFlashPra
	{//闪烁参数
		long		lClr1;						//闪烁颜色1
		long		lClr2;						//闪烁颜色2
		long		lElapse;					//闪烁间隔
		SmtFlashPra()
		{
			lClr1 = RGB(255,0,0);
			lClr2 = RGB(0,255,0);
			lElapse = 100;
		}
	};
			
	struct SmtSysPra
	{//系统参数
		SmtFlashPra	flashPra;					//闪烁参数
		float		fSmargin;					//屏幕拾取容差
		float		fZoomScaleDelt;				//视图放缩速度
		bool		bShowMBR;					//显示MBR
		bool		bShowPoint;					//显示坐标点
		long		lPointRaduis;				//点半径
		long		l2DViewRefreshTime;			//2D视图刷新时间
		long		l2DViewNotifyTime;			//2D视图系统响应时间
		long		l3DViewClearColor;			//3D视图背景颜色
		long		l3DViewRefreshTime;			//3D视图刷新时间
		long		l3DViewNotifyTime;			//3D视图系统响应时间
		string		str2DRenderDeviceName;		//2D视图渲染驱动
		string		str3DRenderDeviceName;		//3D视图渲染驱动

		SmtSysPra():fSmargin(.5)
			,fZoomScaleDelt(0.25)
			,bShowMBR(true)
			,bShowPoint(true)
			,lPointRaduis(4)
			,l2DViewRefreshTime(500)
			,l2DViewNotifyTime(50)
			,l3DViewRefreshTime(50)
			,l3DViewNotifyTime(50)
			,str2DRenderDeviceName("SmtGdiRenderDevice")//SmtGdiRenderDevice
			,str3DRenderDeviceName("OpenGL")//Direct3D,OpenGL
		{
			l3DViewClearColor = RGB(0,0,0);
		}
	};
}

#endif// _SMT_ENV_STRUCT_H