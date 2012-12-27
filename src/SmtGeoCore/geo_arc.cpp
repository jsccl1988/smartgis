
#include <assert.h>
#include <math.h>

#include "geo_geometry.h"
#include "smt_api.h"

using namespace Smt_Core;
using namespace Smt_Base;

namespace Smt_Geo
{
	SmtArc::SmtArc()
	{
		m_fRadius = 0;
	}

	SmtArc::~SmtArc()
	{
		

	}

	// Geometry interface
	int SmtArc::GetDimension() const
	{
		return 2;
	}

	SmtGeometryType SmtArc::GetGeometryType() const
	{
		return GTArc;
	}

	const char *SmtArc::GetGeometryName() const
	{
		return "ARC";
	}

	//////////////////////////////////////////////////////////////////////////
	SmtGeometry *SmtArc::Clone() const
	{
		SmtArc       *poArc;

		poArc = new SmtArc();
		poArc->SetCenterPoint(m_oPointCenter);
		poArc->SetStartPoint(m_oPointStart);
		poArc->SetEndPoint(m_oPointEnd);
		poArc->SetRadius(m_fRadius);

		return poArc;
	}

	void SmtArc::Empty()
	{
		m_fRadius = 0;
	}

	void SmtArc::GetEnvelope( Envelope * psEnvelope ) const
	{

		if (NULL == psEnvelope)
		{
			return;
		}

		psEnvelope->Merge(m_oPointCenter.x-m_fRadius,m_oPointCenter.y-m_fRadius);
		psEnvelope->Merge(m_oPointCenter.x+m_fRadius,m_oPointCenter.y+m_fRadius);
	}

	bool SmtArc::IsEmpty() const
	{
		return (m_fRadius == 0);
	}

	// Curve methods
	double SmtArc::GetLength() const
	{
		double      dfLength = 0;
		double ax,ay,bx,by;
		double theta = 0.;
		ax = m_oPointStart.x - m_oPointCenter.x;
		ay = m_oPointStart.y - m_oPointCenter.y;

		bx = m_oPointEnd.x - m_oPointCenter.x;
		by = m_oPointEnd.y - m_oPointCenter.y;

		theta = acos((ax *bx + ay*by)/m_fRadius/m_fRadius);

        dfLength = m_fRadius*theta*dPI;

		return dfLength;
	}

	void SmtArc::StartPoint(SmtPoint *poPoint) const
	{
		poPoint->SetX(m_oPointStart.x);
		poPoint->SetY(m_oPointStart.y);
	}

	void SmtArc::EndPoint(SmtPoint *poPoint) const
	{
		poPoint->SetX(m_oPointEnd.x);
		poPoint->SetY(m_oPointEnd.y);
	}

	void SmtArc::Value( double dfDistance, SmtPoint * poPoint ) const
	{
		double      dfLength = 0;
		if( dfDistance < 0 )
		{
			StartPoint( poPoint );
			return;
		}

		EndPoint( poPoint );
	}

	// SpatialRelation
	bool SmtArc::Equals( const SmtGeometry * poOther) const
	{
		SmtArc       *poArc = (SmtArc *) poOther;

		if( poArc == this )
			return true;

		if( poArc->GetGeometryType() != GetGeometryType() )
			return false;

		if (IsEqual(poArc->GetRadius(),m_fRadius,dEPSILON))
		    return false;
		
		// we should eventually test the SRS.
		SmtPoint oCenter,oStart,oEnd;

		poArc->GetCenterPoint(&oCenter);
		poArc->StartPoint(&oStart);
		poArc->EndPoint(&oEnd);


		return ( IsEqual(m_oPointCenter.x,oCenter.GetX(),dEPSILON)  && IsEqual(m_oPointCenter.x,oCenter.GetX(),dEPSILON) 
			  && IsEqual(m_oPointStart.x,oStart.GetX(),dEPSILON)  && IsEqual(m_oPointStart.x,oStart.GetX(),dEPSILON)
			  && IsEqual(m_oPointEnd.x,oEnd.GetX(),dEPSILON)  && IsEqual(m_oPointEnd.x,oEnd.GetX(),dEPSILON));
		
		return true;
	}

	// non standard.
	void SmtArc::SetCoordinateDimension( int nDimension )
	{
		m_nCoordDimension = nDimension;
	}
}