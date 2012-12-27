#include "stdafx.h"
#include "sta_diagramdata.h"
#include "sde_datasourcemgr.h"

using namespace Smt_SDEDevMgr;

namespace Smt_StaDiagram
{
	SmtDiagramData::SmtDiagramData():m_pMemDS(NULL)
	{
		SmtDataSourceMgr *pDSMgr = SmtDataSourceMgr::GetSingletonPtr();
		m_pMemDS = pDSMgr->CreateTmpDataSource(eDSType::DS_MEM);
	}

	SmtDiagramData::~SmtDiagramData()
	{
		Clear();

		SmtDataSourceMgr *pDSMgr = SmtDataSourceMgr::GetSingletonPtr();
		pDSMgr->DestoryTmpDataSource(m_pMemDS);
	}

	long SmtDiagramData::Init()
	{
		return SMT_ERR_NONE;
	}

	long SmtDiagramData::Clear()
	{
		return SMT_ERR_NONE;
	}

	SmtMap *SmtDiagramData::GetSmtMapPtr(void)
	{
		return &m_smtMap;
	}

	const SmtMap * SmtDiagramData::GetSmtMapPtr(void) const
	{
		return &m_smtMap;
	}

	SmtMap& SmtDiagramData::GetSmtMap(void)
	{
		return m_smtMap;
	}

	const SmtMap& SmtDiagramData::GetSmtMap(void) const
	{
		return m_smtMap;
	}

	SmtLayer* SmtDiagramData::CreateLayer(const char *szName,fRect &lyrRect,SmtFeatureType ftType)
	{
		SmtLayer *pLayer = m_pMemDS->CreateVectorLayer(szName,lyrRect,ftType);

		if (NULL != pLayer)
			m_smtMap.AddLayer(pLayer);
		 
		return pLayer;
	}

	SmtLayer* SmtDiagramData::GetLayer(const char *szLyrName)
	{
		return m_smtMap.GetLayer(szLyrName);
	}

	const SmtLayer* SmtDiagramData::GetLayer(const char *szLyrName) const
	{
		return m_smtMap.GetLayer(szLyrName);
	}

	long SmtDiagramData::DeleteLayer(const char *szLyrName)
	{
		return (m_smtMap.DeleteLayer(szLyrName))?SMT_ERR_NONE:SMT_ERR_FAILURE;
	}
}
