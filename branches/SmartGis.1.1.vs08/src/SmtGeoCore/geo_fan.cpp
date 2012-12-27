#include <math.h>

#include "smt_api.h"
#include "geo_geometry.h"

using namespace Smt_Core;
using namespace Smt_Base;

namespace Smt_Geo
{
	SmtFan::SmtFan()
	{
		m_pArc = NULL;
	}

	SmtFan::~SmtFan()
	{
		Empty();
	}

	// Non standard (Geometry).
	const char * SmtFan::GetGeometryName() const
	{
		return "FAN";
	}

	SmtGeometryType SmtFan::GetGeometryType() const
	{
		return GTFan;
	}


	SmtGeometry *SmtFan::Clone() const
	{
		SmtFan  *poNewFan;
		poNewFan = new SmtFan();
		poNewFan->SetArc(m_pArc);	 
		return poNewFan;
	}

	void SmtFan::Empty()
	{
		 SMT_SAFE_DELETE(m_pArc);
	}

	bool SmtFan::IsEmpty() const
	{
		if (!m_pArc)
			return false;

		return m_pArc->IsEmpty();
	}

	// Surface Interface
	double SmtFan::GetArea() const
	{
		double dfArea = 0.0;
		double ax,ay,bx,by;
		double theta = 0.;
		double fRadius = 0.;

		SmtPoint oCenter,oStart,oEnd;

		m_pArc->GetCenterPoint(&oCenter);
		m_pArc->StartPoint(&oStart);
		m_pArc->EndPoint(&oEnd);
		fRadius = m_pArc->GetRadius();

		ax = oStart.GetX() - oCenter.GetX();
		ay = oStart.GetY() - oCenter.GetY();

		bx = oEnd.GetX() - oCenter.GetX();
		by = oEnd.GetY() - oCenter.GetY();

		theta = acos((ax *bx + ay*by)/fRadius/fRadius);

		dfArea = fRadius*fRadius*theta/2.;

		 
		return dfArea;
	}

	int SmtFan::Centroid( SmtPoint * poPoint ) const
	{
		if( poPoint == NULL )
			return SMT_ERR_FAILURE;

		return SMT_ERR_UNSUPPORTED;
	}

	int SmtFan::PointOnSurface( SmtPoint * poPoint ) const
	{
		if( poPoint == NULL )
			return SMT_ERR_FAILURE;

		return SMT_ERR_UNSUPPORTED;
	}

	// Geometry
	int  SmtFan::GetDimension() const
	{
		return 2;
	}

	void SmtFan::GetEnvelope( Envelope * psEnvelope ) const
	{
		if (m_pArc)
			m_pArc->GetEnvelope(psEnvelope);
	}

	// SpatialRelation
	bool SmtFan::Equals( const SmtGeometry * poOther) const
	{
		SmtFan *poFan = (SmtFan *) poOther;

		if( poFan == this )
			return TRUE;

		if( poOther->GetGeometryType() != GetGeometryType() )
			return FALSE;

		return poFan->GetArc()->Equals(m_pArc);
	}

	// Non standard
	void SmtFan::SetCoordinateDimension( int nDimension )
	{
		 m_pArc->SetCoordinateDimension(nDimension);

		SmtGeometry::SetCoordinateDimension( nDimension );
	}


	void SmtFan::SetArc( SmtArc * poArc)
	{
		Empty();

		m_pArc = new SmtArc();
		SmtPoint oCenter,oStart,oEnd;	
		poArc->GetCenterPoint(&oCenter);
		poArc->StartPoint(&oStart);
		poArc->EndPoint(&oEnd);

		RawPoint oCt(oCenter.GetX(),oCenter.GetY()),
			     oSt(oStart.GetX(),oStart.GetY()),
			     oEd(oEnd.GetX(),oEnd.GetY());

		m_pArc->SetCenterPoint(oCt);
		m_pArc->SetStartPoint(oSt);
		m_pArc->SetEndPoint(oEd);
		m_pArc->SetRadius(poArc->GetRadius());
	}

	void SmtFan::SetArcDirectly(SmtArc * poArc)
	{
		Empty();
		m_pArc = poArc;
	}

}