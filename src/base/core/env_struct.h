/*
File:    smt_env_struct.h

Desc:    SmartGis��������ͷ�ļ�

Version: Version 1.0

Writter:  �´���

Date:    2010.11.17

Copyright (c) 2010 CCL. All rights reserved.
*/
#ifndef _SMT_ENV_STRUCT_H
#define _SMT_ENV_STRUCT_H

#include "base/core/core.h"

#define MAX_STYLENAME_LENGTH         MAX_NAME_LENGTH
#define MAX_MAPNAME_LENGTH           MAX_NAME_LENGTH

#define SmtPrjHead                   (SmartVersion+0x0001)

namespace base
{
	struct SmtStyleConfig
	{//ϵͳ�㡢�ߡ���Ĭ�Ϸ��
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
	{//ϵͳ�ĵ���Ϣ
		char szMapName[MAX_MAPNAME_LENGTH];
		SmtMapDocInfo()
		{
             sprintf(szMapName,"DefMap");
		}
	};

	struct SmtPrjInfo
	{//ϵͳ������Ϣ
		int           head;
		SmtMapDocInfo mapDocInfo;
		char          szMapDocPath[MAX_PATH];
	};

	struct SmtFlashPra
	{//��˸����
		long		lClr1;						//��˸��ɫ1
		long		lClr2;						//��˸��ɫ2
		long		lElapse;					//��˸���
		SmtFlashPra()
		{
			lClr1 = RGB(255,0,0);
			lClr2 = RGB(0,255,0);
			lElapse = 100;
		}
	};
			
	struct SmtSysPra
	{//ϵͳ����
		SmtFlashPra	flashPra;					//��˸����
		float		fSmargin;					//��Ļʰȡ�ݲ�
		float		fZoomScaleDelt;				//��ͼ�����ٶ�
		bool		bShowMBR;					//��ʾMBR
		bool		bShowPoint;					//��ʾ�����
		long		lPointRaduis;				//��뾶
		long		l2DViewRefreshTime;			//2D��ͼˢ��ʱ��
		long		l2DViewNotifyTime;			//2D��ͼϵͳ��Ӧʱ��
		long		l3DViewClearColor;			//3D��ͼ������ɫ
		long		l3DViewRefreshTime;			//3D��ͼˢ��ʱ��
		long		l3DViewNotifyTime;			//3D��ͼϵͳ��Ӧʱ��
		string		str2DRenderDeviceName;		//2D��ͼ��Ⱦ����
		string		str3DRenderDeviceName;		//3D��ͼ��Ⱦ����

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