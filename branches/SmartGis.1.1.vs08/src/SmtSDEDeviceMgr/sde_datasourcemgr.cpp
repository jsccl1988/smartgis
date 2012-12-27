#include "sde_datasourcemgr.h"
#include "smt_api.h"
#include "sde_smf.h"
#include "sde_ado.h"
#include "sde_mem.h"
#include "sde_ws.h"
//#include "smt_xml.h"

using namespace Smt_SDESmf;
using namespace Smt_SDEAdo;
using namespace Smt_SDEMem;
using namespace Smt_SDEWS;
using namespace Smt_Core;

namespace Smt_SDEDevMgr
{
	SmtDataSourceMgr* SmtDataSourceMgr::m_pSingleton = NULL;

	SmtDataSourceMgr* SmtDataSourceMgr::GetSingletonPtr(void)
	{
		if (m_pSingleton == NULL)
		{
			m_pSingleton = new SmtDataSourceMgr();
		}

		return m_pSingleton;
	}

	void SmtDataSourceMgr::DestoryInstance(void)
	{
		SMT_SAFE_DELETE(m_pSingleton);
	}

	//////////////////////////////////////////////////////////////////////////
	SmtLayer * SmtDataSourceMgr::CreateMemLayer(SmtLayerType eLyrType)
	{
		if(eLyrType == LYR_VECTOR)
			return CreateMemVecLayer();
		else if(eLyrType == LYR_RASTER)
			return CreateMemRasLayer();

		return NULL;
	}

	void SmtDataSourceMgr::DestoryMemLayer(SmtLayer *&pLayer)
	{
		SMT_SAFE_DELETE(pLayer);
	}

	//////////////////////////////////////////////////////////////////////////
	SmtVectorLayer * SmtDataSourceMgr::CreateMemVecLayer(void)
	{
		SmtVectorLayer *pLayer = NULL;
		SmtDataSource *pDS = new SmtMemDataSource();

		if (pDS)
		{
			fRect lyrRect;
			lyrRect.lb.x = 0;
			lyrRect.lb.y = 0;
			lyrRect.rt.x = 500;
			lyrRect.rt.y = 500;

			pLayer = pDS->CreateVectorLayer("SmtMemVecLayer",lyrRect,SmtFeatureType::SmtFtUnknown);
		}

		SMT_SAFE_DELETE(pDS);
		return pLayer;
	}

	void SmtDataSourceMgr::DestoryMemVecLayer(SmtVectorLayer *&pLayer)
	{
		SMT_SAFE_DELETE(pLayer);
	}

	//////////////////////////////////////////////////////////////////////////
	SmtRasterLayer * SmtDataSourceMgr::CreateMemRasLayer(void)
	{
		SmtRasterLayer *pLayer = NULL;
		SmtDataSource *pDS = new SmtMemDataSource();
		if (pDS)
		{
			fRect lyrRect;
			lyrRect.lb.x = 0;
			lyrRect.lb.y = 0;
			lyrRect.rt.x = 500;
			lyrRect.rt.y = 500;

			pLayer = pDS->CreateRasterLayer("SmtMemRasLayer",lyrRect,SmtFeatureType::SmtFtUnknown);
		}

		SMT_SAFE_DELETE(pDS);
		return pLayer;
	}

	void SmtDataSourceMgr::DestoryMemRasLayer(SmtRasterLayer *&pLayer)
	{
		SMT_SAFE_DELETE(pLayer);
	}

	//////////////////////////////////////////////////////////////////////////
	SmtTileLayer * SmtDataSourceMgr::CreateMemTileLayer(void)
	{
		SmtTileLayer *pLayer = NULL;
		SmtDataSource *pDS = new SmtMemDataSource();
		if (pDS)
		{
			fRect lyrRect;
			lyrRect.lb.x = 0;
			lyrRect.lb.y = 0;
			lyrRect.rt.x = 500;
			lyrRect.rt.y = 500;

			pLayer = pDS->CreateTileLayer("SmtMemTileLayer",lyrRect,SmtFeatureType::SmtFtUnknown);
		}

		SMT_SAFE_DELETE(pDS);

		return pLayer;
	}

	void SmtDataSourceMgr::DestoryMemTileLayer(SmtTileLayer *&pLayer)
	{
		SMT_SAFE_DELETE(pLayer);
	}
	
	//////////////////////////////////////////////////////////////////////////
	SmtDataSourceMgr::SmtDataSourceMgr(void)
	{
	   m_pActiveDS = NULL;
	   m_nIteratorIndex = 0;
	   m_strDSMFilePath = "";
	}

	SmtDataSourceMgr::~SmtDataSourceMgr(void)
	{
		Save();

		vector<SmtDataSource*>::iterator iter = m_vSmtDataSources.begin() ;

		while (iter != m_vSmtDataSources.end())
		{
			SMT_SAFE_DELETE(*iter);
			++iter;
		}
		m_vSmtDataSources.clear();

		m_pActiveDS = NULL;
	}

	SmtDataSource* SmtDataSourceMgr::GetDataSource(const char *szName)
	{
		SmtDataSource* pDS = NULL;
		vector<SmtDataSource*>::iterator iter = m_vSmtDataSources.begin() ;

		while (iter != m_vSmtDataSources.end())
		{
			if (strcmp((*iter)->GetName(),szName) == 0)
			{
				pDS = (*iter);
				break;
			}
			++iter;
		}

		return pDS;
	}

	//////////////////////////////////////////////////////////////////////////
	SmtDataSource * SmtDataSourceMgr::CreateTmpDataSource(eDSType type)
	{
		SmtDataSource *pDS = NULL;
		switch(type)
		{
		case DS_DB_ADO:
			{
				pDS = new SmtAdoDataSource();
			}
			break;
		case DS_FILE_SMF:
			{
				pDS = new SmtSmfDataSource();
			}
			break;
		case DS_MEM:
			{
				pDS = new SmtMemDataSource();
			}
			break;
		case DS_WS:
			{
				pDS = new SmtWSDataSource();
			}
			break;
		default:
			break;
		}

		return pDS;
	}

	void SmtDataSourceMgr::DestoryTmpDataSource(SmtDataSource *& pTmpFileDS)
	{
		SMT_SAFE_DELETE(pTmpFileDS);
	}

	//////////////////////////////////////////////////////////////////////////

	SmtDataSource * SmtDataSourceMgr::CreateDataSource(SmtDataSourceInfo &info)
	{
		if (strlen(info.szName) == 0)
			return NULL;
		 
		if (GetDataSource(info.szName) != NULL)
		{
			return NULL;
		}

		SmtDataSource *pDS = NULL;
		switch(info.unType)
		{
		case DS_DB_ADO:
			{
				pDS = new SmtAdoDataSource();
				sprintf_s(info.szUrl,MAX_URL_LENGTH,"sdb:%s\\%s,%s,%s,%s,%d,%d",info.szName,info.db.szService,info.db.szDBName,info.szUID,info.szPWD,info.unType,info.unProvider);
			}
			break;
		case DS_FILE_SMF:
			{
				pDS = new SmtSmfDataSource();
				sprintf_s(info.szUrl,MAX_URL_LENGTH,"sfile:%s\\%s,%s,%s,%s,%d,%d",info.szName,info.file.szPath,info.file.szFileName,info.szUID,info.szPWD,info.unType,info.unProvider);
			}
			break;
		case DS_MEM:
			{
				pDS = new SmtMemDataSource();
				sprintf_s(info.szUrl,MAX_URL_LENGTH,"smem:%s\\%s,%s,%d,%d",info.szName,info.szUID,info.szPWD,info.unType,info.unProvider);
			}
			break;
		case DS_WS:
			{
				pDS = new SmtWSDataSource();
				sprintf_s(info.szUrl,MAX_URL_LENGTH,"sws:%s\\%s,%s,%d,%d",info.szName,info.szUID,info.szPWD,info.unType,info.unProvider);
			}
			break;
		default:
			break;
		}

		if (NULL != pDS)
		{
			pDS->SetInfo(info);
			
			if (!pDS->Open())
			{
				SMT_SAFE_DELETE(pDS);
			}
			else 
			{
				pDS->Close();
				m_vSmtDataSources.push_back(pDS);
			}
		}

		return pDS;
	}

	bool SmtDataSourceMgr::DeleteDataSource(const char *szName)
	{
		bool bRet = true;
		vector<SmtDataSource*>::iterator iter = m_vSmtDataSources.begin() ;
		while (iter != m_vSmtDataSources.end())
		{
			if (strcmp((*iter)->GetName(),szName) == 0)
			{
				if ((*iter) == m_pActiveDS)
					m_pActiveDS = NULL;

				SMT_SAFE_DELETE(*iter);
				break;
			}
			iter++;
		}

		if (iter != m_vSmtDataSources.end())
		{
			m_vSmtDataSources.erase(iter);

			bRet = true;
		}

		return bRet;
	}

	void SmtDataSourceMgr::SetActiveDataSource(const char *szActiveDSName)
	{
		m_pActiveDS = GetDataSource(szActiveDSName);
	}

	//////////////////////////////////////////////////////////////////////////
	void SmtDataSourceMgr::MoveFirst(void)
	{
		m_nIteratorIndex = 0;
	}

	void SmtDataSourceMgr::MoveNext(void)
	{
		if (m_nIteratorIndex < m_vSmtDataSources.size())
			m_nIteratorIndex++;
	}

	void SmtDataSourceMgr::MoveLast(void)
	{
		m_nIteratorIndex = m_vSmtDataSources.size() -1 ;
	}

	void SmtDataSourceMgr::Delete(void)
	{
		SmtDataSource *pSmtDS = m_vSmtDataSources[m_nIteratorIndex];

		SMT_SAFE_DELETE(pSmtDS);

		m_vSmtDataSources.erase( m_vSmtDataSources.begin() + m_nIteratorIndex );
	}

	bool SmtDataSourceMgr::IsEnd(void) 
	{
		return (m_nIteratorIndex == m_vSmtDataSources.size());
	}

	SmtDataSource * SmtDataSourceMgr::GetDataSource()
	{
		return GetDataSource(m_nIteratorIndex);
	}

	SmtDataSource * SmtDataSourceMgr::GetDataSource(int index)
	{
		if (index < 0 || index > (m_vSmtDataSources.size()-1) )
			return NULL;

		return m_vSmtDataSources[index];
	}

	//////////////////////////////////////////////////////////////////////////
	bool SmtDataSourceMgr::Open(const char *szDSMFile)
	{
		if (strlen(szDSMFile) == 0)
			return false;

		m_strDSMFilePath = szDSMFile;

		ifstream infile;

		locale loc = locale::global(locale(".936"));
		infile.open(m_strDSMFilePath.c_str(),ios::out|ios::binary);
		locale::global(std::locale(loc));

		if (!infile.is_open())
		{
			return SMT_ERR_FAILURE;
		}

		//////////////////////////////////////////////////////////////////////////
		//head
		char szHead[4] = "DSM";
		infile.read((char*)(szHead),sizeof(char)*4);
		
		//content
		int nDSs = 0;
		infile.read((char*)(&nDSs),sizeof(int));
		
		SmtDataSourceInfo info;
		for (int i = 0 ; i < nDSs ; i++)
		{
			infile.read((char*)(&info),sizeof(SmtDataSourceInfo));
			CreateDataSource(info);
		}
		//////////////////////////////////////////////////////////////////////////

		infile.close();

		return true;
	}

	bool SmtDataSourceMgr::Save(void)
	{
		if (m_strDSMFilePath == "")
		{
			m_strDSMFilePath = GetAppPath()+"sys\\smartgis.dsm";
		}

		return SaveAs(m_strDSMFilePath.c_str());
	}

	bool SmtDataSourceMgr::SaveAs(const char *szDSMFile)
	{
		if (strlen(szDSMFile) == 0)
			return false;

		ofstream outfile;

		locale loc = locale::global(locale(".936"));
		outfile.open(szDSMFile,ios::out|ios::binary);
		locale::global(std::locale(loc));

		if (!outfile.is_open())
		{
			return false;
		}

		//////////////////////////////////////////////////////////////////////////
		//head
		char szHead[4] = "DSM";
		outfile.write((char*)(szHead),sizeof(char)*4);

		//content
		int nDSs = m_vSmtDataSources.size();
		outfile.write((char*)(&nDSs),sizeof(int));

		SmtDataSourceInfo info;
		vector<SmtDataSource*>::iterator iter = m_vSmtDataSources.begin() ;

		while (iter != m_vSmtDataSources.end())
		{
			(*iter)->GetInfo(info);
			outfile.write((char*)(&info),sizeof(SmtDataSourceInfo));
			++iter;
		}
		//////////////////////////////////////////////////////////////////////////

		outfile.close();

		return true;
	}

}