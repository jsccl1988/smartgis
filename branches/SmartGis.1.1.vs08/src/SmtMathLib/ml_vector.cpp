#include "ml_mathlib.h"

#include <math.h>

namespace Smt_Math
{
	//construct
	Vector::Vector(void)
	{ 
		x=0, y=0; 
	}

	Vector::Vector(float _x, float _y)
	{ 
		x=_x; y=_y;
	}

	void Vector::Set(float _x,float _y)
	{
		x=_x; y=_y;
	}

	//function to be called 
	inline float Vector::GetLength(void) 
	{
	   float f=0;
	   f = (float)sqrt(x*x + y*y); 
	   return f;
	}

	inline float Vector::GetSqrLength(void) const 
	{
	   return (x*x + y*y); 
	}

	inline void Vector::Negate(void)
	{
	   x = -x;  
	   y = -y;;
	}

	inline float Vector::AngleWith(Vector &v) 
	{
	   return (float)acos( ((*this) * v) / (this->GetLength()*v.GetLength()) );
	}

	inline void Vector::Normalize(void) 
	{
		 float f = (float)sqrt(x*x + y*y);

		 if (f != 0.0f) 
		 {
			x/=f; 
			y/=f;;
		 }
	}

	void Vector::Rotate(const Vector &vAxis,float theta) 
	{
		// Calculate the sine and cosine of the angle once
		float cosTheta = (float)cos(theta);
		float sinTheta = (float)sin(theta);

		float _x = vAxis.x;
		float _y = vAxis.y;
	
		Vector vNew;
		// Find the new x position for the new rotated point
		vNew.x  = (cosTheta + (1 - cosTheta) * _x * _x)* x;
		vNew.x += ((1 - cosTheta) * _x * _y )* y;
		
		// Find the new y position for the new rotated point
		vNew.y  = ((1 - cosTheta) * _x * _y )* x;
		vNew.y += (cosTheta + (1 - cosTheta) * _y * _y)* y;

		*this = vNew; 
	}

	inline double Vector::CrossProduct(const Vector &v1) // cross product
	{
		return x*v1.y - y*v1.x;
	}

	/**
	 * Get vector from v1 to v2. 
	 */
	inline void Vector::Difference(const Vector &v1, const Vector &v2) 
	{
	   x = v2.x - v1.x;
	   y = v2.y - v1.y;
	}

	void Vector::operator += (const Vector &v) 
	{
	   x += v.x;   y += v.y;
	}

	Vector Vector::operator + (const Vector &v) const 
	{
	   return Vector(x+v.x, y+v.y);
	}

	void Vector::operator -= (const Vector &v) 
	{
	   x -= v.x;   
	   y -= v.y; 
	}

	Vector Vector::operator - (const Vector &v) const 
	{
	   return Vector(x-v.x, y-v.y);
	}

	void Vector::operator *= (float f) 
	{
	   x *= f;   y *= f;
	}

	void Vector::operator /= (float f) 
	{
	   x /= f;   y /= f;
	}

	Vector Vector::operator * (float f) const 
	{
	   return Vector(x*f, y*f); 
	}

	Vector Vector::operator / (float f) const 
	{
	   return Vector(x/f, y/f);
	}

	void Vector::operator += (float f) 
	{
	   x += f;   y += f; 
	}

	void Vector::operator -= (float f) 
	{
	   x -= f;   y -= f; 
	}

	Vector Vector::operator + (float f) const 
	{
	   return Vector(x+f, y+f);
	}

	Vector Vector::operator - (float f) const 
	{
	   return Vector(x-f, y-f);
	}

	float Vector::operator * (const Vector &v) const
	{
	   return (v.x*x + v.y*y );
	}
}