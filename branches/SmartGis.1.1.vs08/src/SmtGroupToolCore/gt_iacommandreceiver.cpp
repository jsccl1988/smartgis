#include "gt_iacommandreceiver.h"
#include "gis_map.h"
#include "smt_api.h"
#include "bl_api.h"

using namespace Smt_GIS;

namespace Smt_GroupTool
{	
	SmtIACommandReceiver::SmtIACommandReceiver(LPRENDERDEVICE	pRenderDevice,SmtMap *pOperMap,SmtFeature *pFeature,SmtIAType iaType):m_pRenderDevice(pRenderDevice)
		,m_pOperMap(pOperMap)
		,m_pFeature(pFeature)
		,m_iaType(iaType)
	{

	}
	
	SmtIACommandReceiver::~SmtIACommandReceiver()
	{

	}

	bool SmtIACommandReceiver::Action(bool bUndo)
	{
		switch(m_iaType)
		{
		case eSmtIA_AppendFeature:
			{
				if (bUndo)
				{
					m_pFeature = m_pFeature->Clone();
					m_pOperMap->DeleteFeature(m_pFeature);
				}
				else
				{
					m_pOperMap->AppendFeature(m_pFeature,true);
				}
			}
			break;
		case  eSmtIA_DeleteFeature:
			{
				if (bUndo)
				{
					m_pOperMap->AppendFeature(m_pFeature,true);
				}
				else
				{
					m_pFeature = m_pFeature->Clone();
					m_pOperMap->DeleteFeature(m_pFeature);
				}
			}
			break;
		case  eSmtIA_ModifyFeature:
			{
				if (bUndo)
				{
					m_pOperMap->UpdateFeature(m_pFeature);
				}
			}
		    break;
		default:
		    break;
		}

		//refresh
		fRect frt;
		Envelope envelope ;
		float fMargin = 5./m_pRenderDevice->GetBlc();
		const SmtGeometry *pGeom = m_pFeature->GetGeometryRef();

		pGeom->GetEnvelope(&envelope);
		envelope.Merge(envelope.MinX-fMargin,envelope.MinY-fMargin);
		envelope.Merge(envelope.MaxX+fMargin,envelope.MaxY+fMargin);
		EnvelopeToRect(frt,envelope);

		m_pRenderDevice->Refresh(m_pOperMap,frt);

		return true;
	}
}