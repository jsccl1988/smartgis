#include "bl_envelope.h"

namespace Smt_Base
{
	Envelope::Envelope()
	{
		MinX = MaxX = MinY = MaxY = SMT_C_INVALID_DBF_VALUE;
	}

	Envelope::~Envelope()
	{

	}

	bool  Envelope::IsInit() const
	{ 
		return (MinX != SMT_C_INVALID_DBF_VALUE || MinY != SMT_C_INVALID_DBF_VALUE || 
			    MaxX != SMT_C_INVALID_DBF_VALUE || MaxY != SMT_C_INVALID_DBF_VALUE); 
	}

	void Envelope::Merge( Envelope const& sOther ) 
	{
		if( IsInit() && sOther.IsInit() )
		{
			MinX = min(MinX,sOther.MinX);
			MaxX = max(MaxX,sOther.MaxX);
			MinY = min(MinY,sOther.MinY);
			MaxY = max(MaxY,sOther.MaxY);
		}
		else
		{
			MinX = sOther.MinX;
			MaxX = sOther.MaxX;
			MinY = sOther.MinY;
			MaxY = sOther.MaxY;
		}
	}
	void Envelope::Merge( double dfX, double dfY ) {
		if( IsInit() )
		{
			MinX = min(MinX,dfX);
			MaxX = max(MaxX,dfX);
			MinY = min(MinY,dfY);
			MaxY = max(MaxY,dfY);
		}
		else
		{
			MinX = MaxX = dfX;
			MinY = MaxY = dfY;
		}
	}

	void Envelope::Intersect( Envelope const& sOther ) {
		if(Intersects(sOther))
		{
			if( IsInit() )
			{
				MinX = max(MinX,sOther.MinX);
				MaxX = min(MaxX,sOther.MaxX);
				MinY = max(MinY,sOther.MinY);
				MaxY = min(MaxY,sOther.MaxY);
			}
			else
			{
				MinX = sOther.MinX;
				MaxX = sOther.MaxX;
				MinY = sOther.MinY;
				MaxY = sOther.MaxY;
			}
		}
		else
		{
			MinX = 0;
			MaxX = 0;
			MinY = 0;
			MaxY = 0;
		}
	}

	bool Envelope::Intersects(Envelope const& other) const
	{
		return MinX <= other.MaxX && MaxX >= other.MinX && 
			   MinY <= other.MaxY && MaxY >= other.MinY;
	}

	bool Envelope::Contains(Envelope const& other) const
	{
		return MinX <= other.MinX && MinY <= other.MinY &&
			MaxX >= other.MaxX && MaxY >= other.MaxY;
	}

	bool Envelope::Contains(double dfX, double dfY) const
	{
		return MinX <= dfX && MinY <= dfY &&
			MaxX >= dfX && MaxY >= dfY;
	}
}