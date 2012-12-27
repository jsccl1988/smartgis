#include "msvr_mapservercapabilities111.h"
#include "smt_logmanager.h"
#include <iostream>
#include "smt_xml.h"

namespace Smt_MapServer
{
	SmtWMSCapabilities111::SmtWMSCapabilities111(SmtMapService *pMapService):SmtCapabilities(pMapService)
	{

	}

	SmtWMSCapabilities111::~SmtWMSCapabilities111()
	{
		
	}

	long SmtWMSCapabilities111::Create(SmtMapService *pMapService)
	{
		if (NULL != pMapService)
			m_pMapService = pMapService;
		else if (NULL == m_pMapService)
			return SMT_ERR_FAILURE;

		m_doc.Clear();

		return Build();
	}

	/////////////////////////////////////////////////////////////////////////////////////
	long SmtWMSCapabilities111::Build()
	{
		m_cslock.Lock();

		TiXmlDeclaration * decl = new TiXmlDeclaration( "1.0", "UTF-8", "" );
		m_doc.LinkEndChild( decl );

		TiXmlElement * WMT_MS_Capabilities = new TiXmlElement( "WMT_MS_Capabilities" );
		m_doc.LinkEndChild( WMT_MS_Capabilities );

		BuildService(WMT_MS_Capabilities);
		BuildCapability(WMT_MS_Capabilities);

		m_cslock.Unlock();

		return SMT_ERR_NONE;
	}

	long SmtWMSCapabilities111::BuildService(TiXmlElement * Parent)
	{
		TiXmlElement * Service = new TiXmlElement( "Service" );
		Parent->LinkEndChild(Service);

		return SMT_ERR_NONE;
	}

	long SmtWMSCapabilities111::BuildCapability(TiXmlElement * Parent)
	{
		TiXmlElement * Capability = new TiXmlElement( "Capability" );
		Parent->LinkEndChild(Capability);

		BuildRequest(Capability);
		BuildLayer(Capability);

		return SMT_ERR_NONE;
	}

	////////////////////////////////////////////////////////////////////////////////////
	//++
	//service

	//capability
	long SmtWMSCapabilities111::BuildRequest(TiXmlElement * Parent)
	{
		TiXmlElement * Request = new TiXmlElement( "Request" );
		Parent->LinkEndChild(Request);

		BuildGetCapabilities(Request);
		BuildGetMap(Request);

		return SMT_ERR_NONE;
	}

	long SmtWMSCapabilities111::BuildLayer(TiXmlElement * Parent)
	{
		TiXmlElement * Layer = new TiXmlElement( "Layer" );
		Parent->LinkEndChild(Layer);

		BuildSRS(Layer);
		BuildLayers(Layer);

		return SMT_ERR_NONE;
	}

	////////////////////////////////////////////////////////////////////////////////////
	//+++
	//request
	long SmtWMSCapabilities111::BuildGetCapabilities(TiXmlElement * Parent)
	{
		TiXmlElement * GetCapabilities = new TiXmlElement( "GetCapabilities" );
		Parent->LinkEndChild(GetCapabilities);

		return SMT_ERR_NONE;
	}

	long SmtWMSCapabilities111::BuildGetMap(TiXmlElement * Parent)
	{
		TiXmlElement * GetMap = new TiXmlElement( "GetMap" );
		Parent->LinkEndChild(GetMap);

		TiXmlElement * Format = new TiXmlElement( "Format" );
		GetMap->LinkEndChild(Format);

		TiXmlText * textImgCode = new TiXmlText( "image/jpeg" );
		Format->LinkEndChild( textImgCode );

		return SMT_ERR_NONE;
	}

	//+++
	//layer
	long SmtWMSCapabilities111::BuildSRS(TiXmlElement * Parent)
	{
		TiXmlElement * SRS = new TiXmlElement( "SRS" );
		Parent->LinkEndChild(SRS);

		TiXmlText * textSRS = new TiXmlText( "EPSG:4326" );
		SRS->LinkEndChild( textSRS );

		return SMT_ERR_NONE;
	}

	long SmtWMSCapabilities111::BuildLayers(TiXmlElement * Parent)
	{
		SmtMap *pMap = m_pMapService->GetSmtMapPtr();

		pMap->MoveFirst();
		while (!pMap->IsEnd())
		{
			TiXmlElement * Layer = new TiXmlElement( "Layer" );
			Parent->LinkEndChild(Layer);

			BuildLayer(Layer,pMap->GetLayer());
			pMap->MoveNext();
		}

		return SMT_ERR_NONE;
	}

	long SmtWMSCapabilities111::BuildLayer(TiXmlElement * Parent,const SmtLayer * pLayer)
	{
		if (NULL == pLayer)
			return SMT_ERR_FAILURE;

		Envelope env;
		pLayer->GetEnvelope(env);

		TiXmlElement * Name = new TiXmlElement( "Name" );
		Parent->LinkEndChild(Name);

		TiXmlText * textName = new TiXmlText( pLayer->GetLayerName());
		Name->LinkEndChild( textName );

		TiXmlElement * Title = new TiXmlElement( "Title" );
		Parent->LinkEndChild(Title);

		TiXmlText * textTitle = new TiXmlText( pLayer->GetLayerName() );
		Title->LinkEndChild( textTitle );

		TiXmlElement * Abstract = new TiXmlElement( "Abstract" );
		Parent->LinkEndChild(Abstract);

		TiXmlElement * SRS0 = new TiXmlElement( "SRS" );
		Parent->LinkEndChild(SRS0);

		TiXmlText * textSRS0 = new TiXmlText( "EPSG:4326" );
		SRS0->LinkEndChild( textSRS0 );

		TiXmlElement * BoundingBox = new TiXmlElement( "BoundingBox" );
		Parent->LinkEndChild(BoundingBox);
		BoundingBox->SetAttribute("SRS","EPSG:4326");
		BoundingBox->SetDoubleAttribute("minx",env.MinX);
		BoundingBox->SetDoubleAttribute("miny",env.MinY);
		BoundingBox->SetDoubleAttribute("maxx",env.MaxX);
		BoundingBox->SetDoubleAttribute("maxy",env.MaxY);

		TiXmlElement * LatLonBoundingBox = new TiXmlElement( "LatLonBoundingBox" );
		Parent->LinkEndChild(LatLonBoundingBox);
		LatLonBoundingBox->SetAttribute("SRS","EPSG:4326");
		LatLonBoundingBox->SetDoubleAttribute("minx",env.MinX);
		LatLonBoundingBox->SetDoubleAttribute("miny",env.MinY);
		LatLonBoundingBox->SetDoubleAttribute("maxx",env.MaxX);
		LatLonBoundingBox->SetDoubleAttribute("maxy",env.MaxY);

		TiXmlElement * Style = new TiXmlElement( "Style" );
		Parent->LinkEndChild(Style);

		TiXmlElement * StyleName = new TiXmlElement( "Name" );
		Style->LinkEndChild(StyleName);
		TiXmlText * textStyleName = new TiXmlText( "polygon" );
		StyleName->LinkEndChild( textStyleName );

		TiXmlElement * StyleTitle = new TiXmlElement( "Title" );
		Style->LinkEndChild(StyleTitle);
		TiXmlText * textStyleTitle = new TiXmlText( "polygon" );
		StyleTitle->LinkEndChild( textStyleTitle );

		TiXmlElement * StyleAbstract = new TiXmlElement( "Abstract" );
		Style->LinkEndChild(StyleAbstract);
		TiXmlText * textStyleAbstract = new TiXmlText( "Default style for all the polygon layer" );
		StyleAbstract->LinkEndChild( textStyleAbstract );

		return SMT_ERR_NONE;
	}
}