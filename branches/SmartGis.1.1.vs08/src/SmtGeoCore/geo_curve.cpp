#include "geo_geometry.h"

namespace Smt_Geo
{
	SmtCurve::SmtCurve()
	{
	}

	SmtCurve::~SmtCurve()
	{
	}

	bool SmtCurve::IsClosed() const

	{
		SmtPoint            oStartPoint, oEndbfPoint;

		StartPoint( &oStartPoint );
		EndPoint( &oEndbfPoint );

		if( oStartPoint.GetX() == oEndbfPoint.GetX()
			&& oStartPoint.GetY() == oEndbfPoint.GetY() )
		{
			return true;
		}
		else
		{
			return false;
		}
	}
}
