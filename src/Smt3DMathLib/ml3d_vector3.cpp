#include "ml3d_mathlib.h"
namespace Smt_3DMath
{
	//construct
	Vector3::Vector3(void)
	{ 
		x=0, y=0, z=0; 
	}

	Vector3::Vector3(float _x, float _y, float _z)
	{ 
		x=_x; y=_y; z=_z; 
	}

	Vector3::Vector3(Vector4 &vec4)
	{
		Set(vec4.x,vec4.y,vec4.z);
	}

	void Vector3::Set(float _x,float _y,float _z)
	{
		x=_x; y=_y; z=_z;
	}

	//function to be called 
	inline float Vector3::GetLength(void) 
	{
	   float f=0;
	   f = (float)sqrt(x*x + y*y + z*z); 
	   return f;
	}

	inline float Vector3::GetSqrLength(void) const 
	{
	   return (x*x + y*y + z*z); 
	}

	inline void Vector3::Negate(void)
	{
	   x = -x;  y = -y;  z = -z;
	}

	inline float Vector3::AngleWith(Vector3 &v) 
	{
	   return (float)acos( ((*this) * v) / (this->GetLength()*v.GetLength()) );
	}

	inline void Vector3::Normalize(void) 
	{
		 float f = (float)sqrt(x*x + y*y + z*z);

		  if (f != 0.0f) 
		  {
			 x/=f; y/=f; z/=f;
		  }
	 }

	void Vector3::Rotate(const Vector3 &vAxis,float theta) 
	{

		// Calculate the sine and cosine of the angle once
		float cosTheta = (float)cos(theta);
		float sinTheta = (float)sin(theta);

		float _x = vAxis.x;
		float _y = vAxis.y;
		float _z = vAxis.z;

		Vector3 vNew;
		// Find the new x position for the new rotated point
		vNew.x  = (cosTheta + (1 - cosTheta) * _x * _x)		* x;
		vNew.x += ((1 - cosTheta) * _x * _y - _z * sinTheta)* y;
		vNew.x += ((1 - cosTheta) * _x * _z + _y * sinTheta)* z;
		
		// Find the new y position for the new rotated point
		vNew.y  = ((1 - cosTheta) * _x * _y + _z * sinTheta)* x;
		vNew.y += (cosTheta + (1 - cosTheta) * _y * _y)		* y;
		vNew.y += ((1 - cosTheta) * _y * _z - _x * sinTheta)* z;
		
		// Find the new z position for the new rotated point
		vNew.z  = ((1 - cosTheta) * _x * _z - _y * sinTheta)* x;
		vNew.z += ((1 - cosTheta) * _y * _z + _x * sinTheta)* y;
		vNew.z += (cosTheta + (1 - cosTheta) * _z * _z)		* z;

		*this = vNew; 
		
	}

	/**
	 * Get vector from v1 to v2. 
	 */
	inline void Vector3::Difference(const Vector3 &v1, const Vector3 &v2) 
	{
	   x = v2.x - v1.x;
	   y = v2.y - v1.y;
	   z = v2.z - v1.z;
	 }
	/*----------------------------------------------------------------*/

	/**
	 * Build cross product of two Vector3s, use SSE if available. Note
	 * that none of the parameters values is changed.
	 */
	inline Vector3 Vector3::CrossProduct(const Vector3 &v1) 
	{
		Vector3 v2;
		v2.x = y * v1.z - z * v1.y;
		v2.y = z * v1.x - x * v1.z;
		v2.z = x * v1.y - y * v1.x;
		return v2;
	}


	void Vector3::operator += (const Vector3 &v) 
	{
	   x += v.x;   y += v.y;   z += v.z;
	}

	Vector3 Vector3::operator + (const Vector3 &v) const 
	{
	   return Vector3(x+v.x, y+v.y, z+v.z);
	}

	void Vector3::operator -= (const Vector3 &v) 
	{
	   x -= v.x;   y -= v.y;   z -= v.z;
	}

	Vector3 Vector3::operator - (const Vector3 &v) const 
	{
	   return Vector3(x-v.x, y-v.y, z-v.z);
	}

	void Vector3::operator *= (float f) 
	{
	   x *= f;   y *= f;   z *= f;
	}

	void Vector3::operator /= (float f) 
	{
	   x /= f;   y /= f;   z /= f;
	}

	Vector3 Vector3::operator * (float f) const 
	{
	   return Vector3(x*f, y*f, z*f); 
	}

	Vector3 Vector3::operator / (float f) const 
	{
	   return Vector3(x/f, y/f, z/f);
	}

	void Vector3::operator += (float f) 
	{
	   x += f;   y += f;   z += f;
	}

	void Vector3::operator -= (float f) 
	{
	   x -= f;   y -= f;   z -= f;
	}

	Vector3 Vector3::operator + (float f) const 
	{
	   return Vector3(x+f, y+f, z+f);
	}

	Vector3 Vector3::operator - (float f) const 
	{
	   return Vector3(x-f, y-f, z-f);
	}

	float Vector3::operator * (const Vector3 &v) const
	{
	   return (v.x*x + v.y*y + v.z*z);
	}

	Quat Vector3::operator * (const Quat &q) const 
	{
	   return Quat( q.w*x + q.z*y - q.y*z,
					q.w*y + q.x*z - q.z*x,
					q.w*z + q.y*x - q.x*y,
					-(q.x*x + q.y*y + q.z*z));
	}
}