#include "ml3d_mathlib.h"
namespace Smt_3DMath
{
	//construct
	Vector2::Vector2(void)
	{ 
		x=0, y=0; 
	}

	Vector2::Vector2(float _x, float _y)
	{ 
		x=_x; y=_y;
	}

	void Vector2::Set(float _x,float _y)
	{
		x=_x; y=_y;
	}

	//function to be called 
	inline float Vector2::GetLength(void) 
	{
		float f=0;
		f = (float)sqrt(x*x + y*y); 
		return f;
	}

	inline float Vector2::GetSqrLength(void) const 
	{
		return (x*x + y*y); 
	}

	inline void Vector2::Negate(void)
	{
		x = -x;  y = -y;;
	}

	inline float Vector2::AngleWith(Vector2 &v) 
	{
		return (float)acos( ((*this) * v) / (this->GetLength()*v.GetLength()) );
	}

	inline void Vector2::Normalize(void) 
	{
		float f = (float)sqrt(x*x + y*y);

		if (f != 0.0f) 
		{
			x/=f; y/=f;;
		}
	}

	void Vector2::Rotate(const Vector2 &vAxis,float theta) 
	{

		// Calculate the sine and cosine of the angle once
		float cosTheta = (float)cos(theta);
		float sinTheta = (float)sin(theta);

		float _x = vAxis.x;
		float _y = vAxis.y;

		Vector2 vNew;
		// Find the new x position for the new rotated point
		vNew.x  = (cosTheta + (1 - cosTheta) * _x * _x)* x;
		vNew.x += ((1 - cosTheta) * _x * _y )* y;

		// Find the new y position for the new rotated point
		vNew.y  = ((1 - cosTheta) * _x * _y )* x;
		vNew.y += (cosTheta + (1 - cosTheta) * _y * _y)* y;

		*this = vNew; 

	}

	inline double Vector2::CrossProduct(const Vector2 &v1) // cross product
	{
		return x*v1.y - y*v1.x;
	}

	/**
	* Get vector from v1 to v2. 
	*/
	inline void Vector2::Difference(const Vector2 &v1, const Vector2 &v2) 
	{
		x = v2.x - v1.x;
		y = v2.y - v1.y;
	}

	void Vector2::operator += (const Vector2 &v) 
	{
		x += v.x;   y += v.y;

	}

	Vector2 Vector2::operator + (const Vector2 &v) const 
	{
		return Vector2(x+v.x, y+v.y);

	}

	void Vector2::operator -= (const Vector2 &v) 
	{
		x -= v.x;   y -= v.y; 
	}

	Vector2 Vector2::operator - (const Vector2 &v) const 
	{
		return Vector2(x-v.x, y-v.y);

	}

	void Vector2::operator *= (float f) 
	{
		x *= f;   y *= f;

	}

	void Vector2::operator /= (float f) 
	{
		x /= f;   y /= f;

	}

	Vector2 Vector2::operator * (float f) const 
	{
		return Vector2(x*f, y*f); 
	}

	Vector2 Vector2::operator / (float f) const 
	{
		return Vector2(x/f, y/f);
	}

	void Vector2::operator += (float f) 
	{
		x += f;   y += f; 
	}

	void Vector2::operator -= (float f) 
	{
		x -= f;   y -= f; 
	}

	Vector2 Vector2::operator + (float f) const 
	{
		return Vector2(x+f, y+f);
	}

	Vector2 Vector2::operator - (float f) const 
	{
		return Vector2(x-f, y-f);
	}

	float Vector2::operator * (const Vector2 &v) const
	{
		return (v.x*x + v.y*y );
	}
}