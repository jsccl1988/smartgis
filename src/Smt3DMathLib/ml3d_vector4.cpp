#include "ml3d_mathlib.h"

namespace Smt_3DMath
{
	float _fabs(float f) 
	{ 
		if (f<0.0f) return -f; 
		return f; 
	}

	//construct
	Vector4::Vector4(void)
	{ 
		x=0, y=0, z=0, w=1.0f; 
	}

	Vector4::Vector4(float _x, float _y, float _z)
	{ 
		x=_x; y=_y; z=_z; w=1.0f; 
	}


	Vector4::Vector4(float _x, float _y, float _z ,float _w)
	{ 
		x=_x; y=_y; z=_z; w=_w; 
	}

	Vector4::Vector4(Vector3 &vec3)
	{
		Set(vec3.x,vec3.y,vec3.z);
	}

	inline void Vector4::Set(float _x, float _y, float _z, float _w)
	{
	   x=_x; y=_y; z=_z; w=_w;
	} 

	//function to be called 
	inline float Vector4::GetLength(void) 
	{
	   float f=0;
	   f = (float)sqrt(x*x + y*y + z*z); 
	   return f;
	}

	inline float Vector4::GetSqrLength(void) const 
	{
	   return (x*x + y*y + z*z); 
	}

	inline void Vector4::Negate(void)
	{
	   x = -x;  y = -y;  z = -z;
	}

	inline float Vector4::AngleWith(Vector4 &v) 
	{
	   return (float)acos( ((*this) * v) / (this->GetLength()*v.GetLength()) );
	}

	inline void Vector4::Normalize(void) 
	{
		 float f = (float)sqrt(x*x + y*y + z*z);

		  if (f != 0.0f) 
		  {
			 x/=f; y/=f; z/=f;
		  }
	 }

	/**
	 * Get vector from v1 to v2. 
	 */
	inline void Vector4::Difference(const Vector4 &v1, const Vector4 &v2) 
	{
	   x = v2.x - v1.x;
	   y = v2.y - v1.y;
	   z = v2.z - v1.z;
	   w = 1.0f;
	 }
	/*----------------------------------------------------------------*/

	/**
	 * Build cross product of two vectors, use SSE if available. Note
	 * that none of the parameters values is changed.
	 */
	inline Vector4 Vector4::CrossProduct(const Vector4 &v1) 
	{
		Vector4 v2;
		v2.x = y * v1.z - z * v1.y;
		v2.y = z * v1.x - x * v1.z;
		v2.z = x * v1.y - y * v1.x;
		v2.w = 1.0f;
		return v2;
	}

	inline void Vector4::RotateWith(const Matrix &m) 
	{
   
	   float _x = x * m._11 + y * m._21 + z * m._31;
	   float _y = x * m._12 + y * m._22 + z * m._32;
	   float _z = x * m._13 + y * m._23 + z * m._33;
	   x = _x;   y = _y;   z = _z;
	} 


	inline void Vector4::InvRotateWith(const Matrix &m) 
	{
   
	   float _x = x * m._11 + y * m._12 + z * m._13;
	   float _y = x * m._21 + y * m._22 + z * m._23;
	   float _z = x * m._31 + y * m._32 + z * m._33;
	   x = _x;   y = _y;   z = _z;
   
	}

	void Vector4::operator += (const Vector4 &v) 
	{
	   x += v.x;   y += v.y;   z += v.z;
   
	}

	Vector4 Vector4::operator + (const Vector4 &v) const 
	{
	   return Vector4(x+v.x, y+v.y, z+v.z);
   
	}

	void Vector4::operator -= (const Vector4 &v) 
	{
	   x -= v.x;   y -= v.y;   z -= v.z;
   
	}

	Vector4 Vector4::operator - (const Vector4 &v) const 
	{
	   return Vector4(x-v.x, y-v.y, z-v.z);
   
	}

	void Vector4::operator *= (float f) 
	{
	   x *= f;   y *= f;   z *= f;
   
	}

	void Vector4::operator /= (float f) 
	{
	   x /= f;   y /= f;   z /= f;
   
	}

	Vector4 Vector4::operator * (float f) const 
	{
	   return Vector4(x*f, y*f, z*f); 
	}

	Vector4 Vector4::operator / (float f) const 
	{
	   return Vector4(x/f, y/f, z/f);
	}

	void Vector4::operator += (float f) 
	{
	   x += f;   y += f;   z += f;
	}

	void Vector4::operator -= (float f) 
	{
	   x -= f;   y -= f;   z -= f;
	}

	Vector4 Vector4::operator + (float f) const 
	{
	   return Vector4(x+f, y+f, z+f);
	}

	Vector4 Vector4::operator - (float f) const 
	{
	   return Vector4(x-f, y-f, z-f);
	}

	float Vector4::operator * (const Vector4 &v) const
	{
	   return (v.x*x + v.y*y + v.z*z);
	}

	Quat Vector4::operator * (const Quat &q) const 
	{
	   return Quat(  q.w*x + q.z*y - q.y*z,
						q.w*y + q.x*z - q.z*x,
						q.w*z + q.y*x - q.x*y,
					  -(q.x*x + q.y*y + q.z*z) );
	}

	/**
	 * Multiply a Vector4 and a matrix, use SSE if available.
	 */
	Vector4 Vector4::operator * (const Matrix &m) const 
	{
	   Vector4 vcResult;

		  vcResult.x = x*m._11 + y*m._21 + z*m._31 + m._41;
		  vcResult.y = x*m._12 + y*m._22 + z*m._32 + m._42;
		  vcResult.z = x*m._13 + y*m._23 + z*m._33 + m._43;
		  vcResult.w = x*m._14 + y*m._24 + z*m._34 + m._44;
   
		  vcResult.x = vcResult.x/vcResult.w;
		  vcResult.y = vcResult.y/vcResult.w;
		  vcResult.z = vcResult.z/vcResult.w;
		  vcResult.w = 1.0f;
	   return vcResult;
	}
}