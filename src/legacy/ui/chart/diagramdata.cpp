#include "stdafx.h"
#include "legacy/ui/chart/diagramdata.h"
#include "sdb/datasource/mgr/datasourcemgr.h"

using namespace sdb;

namespace ui
{
	SmtDiagramData::SmtDiagramData():m_memDS()
	{
		SmtDataSourceMgr *pDSMgr = SmtDataSourceMgr::get_singleton_ptr();
		m_memDS = pDSMgr->CreateTmpDataSource(eDSType::DS_MEM);
	}

	SmtDiagramData::~SmtDiagramData()
	{
		Clear();

		SmtDataSourceMgr *pDSMgr = SmtDataSourceMgr::get_singleton_ptr();
		pDSMgr->DestoryTmpDataSource(m_memDS);
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

	OGRLayer* SmtDiagramData::CreateLayer(const char *szName,fRect &lyrRect,SmtFeatureType ftType)
	{
		OGRLayer *pLayer = m_memDS.CreateVectorLayer(szName,lyrRect,ftType);

		if (NULL != pLayer) {
			m_smtMap.AddLayer(pLayer);
			return m_smtMap.GetOgrLayer(szName);
		}
		return nullptr;
	}

	OGRLayer* SmtDiagramData::GetLayer(const char *szLyrName)
	{
		return m_smtMap.GetOgrLayer(szLyrName);
	}

	const OGRLayer* SmtDiagramData::GetLayer(const char *szLyrName) const
	{
		return m_smtMap.GetOgrLayer(szLyrName);
	}

	long SmtDiagramData::DeleteLayer(const char *szLyrName)
	{
		return (m_smtMap.DeleteLayer(szLyrName))?SMT_ERR_NONE:SMT_ERR_FAILURE;
	}
}
