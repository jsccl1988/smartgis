/*
File:    sde_datasourcemgr.h

Desc:    SmtDataSourceMgr,数据源管理类

Version: Version 1.0

Writer:  陈春亮

Date:    2010.11.17

Copyright (c) 2010 CCL. All rights reserved.
*/
#ifndef _SMT_DSMGR_H
#define _SMT_DSMGR_H

#include "gis_sde.h"

using namespace Smt_GIS;

namespace Smt_SDEDevMgr
{
	class SMT_EXPORT_CLASS SmtDataSourceMgr
	{
	private:
		SmtDataSourceMgr(void);

	public:
		virtual ~SmtDataSourceMgr(void);

	public:
		static SmtLayer *				CreateMemLayer(SmtLayerType eLyrType);
		static void						DestoryMemLayer(SmtLayer *&pLayer);
	 
		static SmtVectorLayer *			CreateMemVecLayer(void);
		static void						DestoryMemVecLayer(SmtVectorLayer *&pLayer);

		static SmtRasterLayer *			CreateMemRasLayer(void);
		static void						DestoryMemRasLayer(SmtRasterLayer *&pLayer);

		static SmtTileLayer *			CreateMemTileLayer(void);
		static void						DestoryMemTileLayer(SmtTileLayer *&pLayer);

	public:
		static SmtDataSourceMgr*		GetSingletonPtr(void);
		static void						DestoryInstance(void);

	public:
		bool							Open(const char *szDSMFile);
		bool							Save(void);
		bool							SaveAs(const char *szDSMFile);

	public:
		SmtDataSource * 				CreateTmpDataSource(eDSType type);
		void							DestoryTmpDataSource(SmtDataSource *& pTmpFileDS);

		SmtDataSource * 				CreateDataSource(SmtDataSourceInfo &info);
		bool							DeleteDataSource(const char *szName);

	public:
		//////////////////////////////////////////////////////////////////////////
		int								GetDataSourceCount(void) {return m_vSmtDataSources.size();}

		void							MoveFirst(void);
		void							MoveNext(void);
		void							MoveLast(void);
		void							Delete(void);
		bool							IsEnd(void);

		SmtDataSource *					GetDataSource();
		
	public:
		SmtDataSource *					GetDataSource(int index);
		SmtDataSource*					GetDataSource(const char *szName);

		inline	SmtDataSource*			GetActiveDataSource(void) {return m_pActiveDS;}
		void							SetActiveDataSource(const char *szActiveDSName);
		inline void						SetActiveDataSource(SmtDataSource* pActiveDS) 
		{ 
			if (pActiveDS)
			{
				m_pActiveDS = pActiveDS;
			}
		}

	private:
		SmtDataSource					*m_pActiveDS;
		vector<SmtDataSource*>			m_vSmtDataSources;
		int								m_nIteratorIndex;
		string							m_strDSMFilePath;

	private:
		static SmtDataSourceMgr*		m_pSingleton;
	};
}

#if !defined(Export_SmtSDEDeviceMgr)
#if   defined( _DEBUG)
#          pragma comment(lib,"SmtSDEDeviceMgrD.lib")
#       else
#          pragma comment(lib,"SmtSDEDeviceMgr.lib")
#	    endif  
#endif

#endif //_SMT_DSMGR_H