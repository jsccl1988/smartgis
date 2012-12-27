#include "geo_3dgeometry.h"

namespace Smt_3DGeo
{
	Smt3DCurve::Smt3DCurve()
	{
	}

	Smt3DCurve::~Smt3DCurve()
	{
	}

	bool Smt3DCurve::IsClosed() const

	{
		Smt3DPoint            oStartPoint, oEndbfPoint;

		StartPoint( &oStartPoint );
		EndPoint( &oEndbfPoint );

		if( oStartPoint.GetX() == oEndbfPoint.GetX()
			&& oStartPoint.GetY() == oEndbfPoint.GetY()
			&& oStartPoint.GetZ() == oEndbfPoint.GetZ())
		{
			return true;
		}
		else
		{
			return false;
		}
	}
}
