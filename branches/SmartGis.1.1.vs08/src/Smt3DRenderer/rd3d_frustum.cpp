#include "rd3d_base.h"

namespace Smt_3Drd
{
	SmtFrustum::SmtFrustum(void)
	{
		;
	}

	SmtFrustum::~SmtFrustum(void)
	{
        ;
	}

	//get frustum matrix
	void SmtFrustum::GetFrustum(float frustum[6][4])
	{
        memcpy(frustum,m_frustum,24*sizeof(float));
	}
	
	//set frustum matrix
	void SmtFrustum::SetFrustum(float frustum[6][4])
	{
        memcpy(m_frustum,frustum,24*sizeof(float));
	}

	bool SmtFrustum::IsPointIn( float x, float y, float z )
	{
		// Go through all the sides of the frustum
		for(int i = 0; i < 6; i++ )
		{
			// Calculate the plane equation and check if the point is behind a side of the frustum
			if(m_frustum[i][P_A] * x + m_frustum[i][P_B] * y + m_frustum[i][P_C] * z + m_frustum[i][P_D] <= 0)
			{
				// The point was behind a side, so it ISN'T in the frustum
				return false;
			}
		}

		// The point was inside of the frustum (In front of ALL the sides of the frustum)
		return true;
	}

	bool SmtFrustum::IsSphereIn( float x, float y, float z, float radius )
	{
		// Go through all the sides of the frustum
		for(int i = 0; i < 6; i++ )	
		{
			// If the center of the sphere is farther away from the plane than the radius
			if( m_frustum[i][P_A] * x + m_frustum[i][P_B] * y + m_frustum[i][P_C] * z + m_frustum[i][P_D] <= -radius )
			{
				// The distance was greater than the radius so the sphere is outside of the frustum
				return false;
			}
		}
		
		// The sphere was inside of the frustum!
		return true;
	}

	bool SmtFrustum::IsCubeIn( float x, float y, float z, float size )
	{
		for(int i = 0; i < 6; i++ )
		{
			if(m_frustum[i][P_A] * (x - size) + m_frustum[i][P_B] * (y - size) + m_frustum[i][P_C] * (z - size) + m_frustum[i][P_D] >= 0)
			   continue;
			if(m_frustum[i][P_A] * (x + size) + m_frustum[i][P_B] * (y - size) + m_frustum[i][P_C] * (z - size) + m_frustum[i][P_D] >= 0)
			   continue;
			if(m_frustum[i][P_A] * (x - size) + m_frustum[i][P_B] * (y + size) + m_frustum[i][P_C] * (z - size) + m_frustum[i][P_D] >= 0)
			   continue;
			if(m_frustum[i][P_A] * (x + size) + m_frustum[i][P_B] * (y + size) + m_frustum[i][P_C] * (z - size) + m_frustum[i][P_D] >= 0)
			   continue;
			if(m_frustum[i][P_A] * (x - size) + m_frustum[i][P_B] * (y - size) + m_frustum[i][P_C] * (z + size) + m_frustum[i][P_D] >= 0)
			   continue;
			if(m_frustum[i][P_A] * (x + size) + m_frustum[i][P_B] * (y - size) + m_frustum[i][P_C] * (z + size) + m_frustum[i][P_D] >= 0)
			   continue;
			if(m_frustum[i][P_A] * (x - size) + m_frustum[i][P_B] * (y + size) + m_frustum[i][P_C] * (z + size) + m_frustum[i][P_D] >= 0)
			   continue;
			if(m_frustum[i][P_A] * (x + size) + m_frustum[i][P_B] * (y + size) + m_frustum[i][P_C] * (z + size) + m_frustum[i][P_D] >= 0)
			   continue;

			// If we get here, it isn't in the frustum
			return false;
		}

		return true;
	}

	bool SmtFrustum::IsCuboidIn( float x, float y, float z, SIZE size )
	{
		for(int i = 0; i < 6; i++ )
		{
			if(m_frustum[i][P_A] * (x - size.cx) + m_frustum[i][P_B] * (y - size.cy) + m_frustum[i][P_C] * (z - size.cx) + m_frustum[i][P_D] > 0)
			   continue;
			if(m_frustum[i][P_A] * (x + size.cx) + m_frustum[i][P_B] * (y - size.cy) + m_frustum[i][P_C] * (z - size.cx) + m_frustum[i][P_D] > 0)
			   continue;
			if(m_frustum[i][P_A] * (x - size.cx) + m_frustum[i][P_B] * (y + size.cy) + m_frustum[i][P_C] * (z - size.cx) + m_frustum[i][P_D] > 0)
			   continue;
			if(m_frustum[i][P_A] * (x + size.cx) + m_frustum[i][P_B] * (y + size.cy) + m_frustum[i][P_C] * (z - size.cx) + m_frustum[i][P_D] > 0)
			   continue;
			if(m_frustum[i][P_A] * (x - size.cx) + m_frustum[i][P_B] * (y - size.cy) + m_frustum[i][P_C] * (z + size.cx) + m_frustum[i][P_D] > 0)
			   continue;
			if(m_frustum[i][P_A] * (x + size.cx) + m_frustum[i][P_B] * (y - size.cy) + m_frustum[i][P_C] * (z + size.cx) + m_frustum[i][P_D] > 0)
			   continue;
			if(m_frustum[i][P_A] * (x - size.cx) + m_frustum[i][P_B] * (y + size.cy) + m_frustum[i][P_C] * (z + size.cx) + m_frustum[i][P_D] > 0)
			   continue;
			if(m_frustum[i][P_A] * (x + size.cx) + m_frustum[i][P_B] * (y + size.cy) + m_frustum[i][P_C] * (z + size.cx) + m_frustum[i][P_D] > 0)
			   continue;

			// If we get here, it isn't in the frustum
			return false;
		}

		return true;
	}

	bool SmtFrustum::IsBoxIn( float max_x, float max_y, float max_z,
		float min_x, float min_y, float min_z)
	{
		for(int i = 0; i < 6; i++ )
		{
			if(m_frustum[i][P_A] * min_x + m_frustum[i][P_B] * min_y + m_frustum[i][P_C] * min_z + m_frustum[i][P_D] > 0)
				continue;
			if(m_frustum[i][P_A] * max_x + m_frustum[i][P_B] * min_y + m_frustum[i][P_C] * min_z + m_frustum[i][P_D] > 0)
				continue;
			if(m_frustum[i][P_A] * min_x + m_frustum[i][P_B] * max_y + m_frustum[i][P_C] * min_z + m_frustum[i][P_D] > 0)
				continue;
			if(m_frustum[i][P_A] * max_x + m_frustum[i][P_B] * max_y + m_frustum[i][P_C] * min_z + m_frustum[i][P_D] > 0)
				continue;
			if(m_frustum[i][P_A] * min_x + m_frustum[i][P_B] * min_y + m_frustum[i][P_C] * max_z + m_frustum[i][P_D] > 0)
				continue;
			if(m_frustum[i][P_A] * max_x + m_frustum[i][P_B] * min_y + m_frustum[i][P_C] * max_z + m_frustum[i][P_D] > 0)
				continue;
			if(m_frustum[i][P_A] * min_x + m_frustum[i][P_B] * max_y + m_frustum[i][P_C] * max_z + m_frustum[i][P_D] > 0)
				continue;
			if(m_frustum[i][P_A] * max_x + m_frustum[i][P_B] * max_y + m_frustum[i][P_C] * max_z + m_frustum[i][P_D] > 0)
				continue;
			
			// If we get here, it isn't in the frustum
			return false;
		}
		
		return true;
	}
}