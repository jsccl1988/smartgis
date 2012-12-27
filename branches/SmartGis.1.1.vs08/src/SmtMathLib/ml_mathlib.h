/*
File:    mathlib.h

Desc:    ¶þÎ¬ÊýÑ§¿â

Version: Version 1.0

Writter:  ³Â´ºÁÁ

Date:    2010.11.17

Copyright (c) 2010 CCL. All rights reserved.
*/
#ifndef _SMT_MATH_LIB_H
#define _SMT_MATH_LIB_H

#include "smt_core.h"

namespace Smt_Math
{
	class SMT_EXPORT_CLASS Vector
	{
	public:
		union
		{
			struct
			{
				float x, y;
			};
			float v[2];
		};
	public:
		//construct
		Vector(void) ;
		Vector(float _x,float _y);
		
		//function
		inline void				Set(float _x,float _y);
		inline float			GetLength(void);					// length
		inline float			GetSqrLength(void) const;			// square length
		inline void				Negate(void);						// Vector4 mult -1
		inline void				Normalize(void);					// normalize
		inline float			AngleWith( Vector &v);             // angle in rad
		inline void				Difference(const Vector &v1,const Vector &v2); // from v1 to v2
		void					Rotate(const Vector &vAxis,float theta);		//rotate by an axis 
		inline double			CrossProduct(const Vector &v1); // cross product
		
		void operator			+= (const Vector &v);				// operator +=
		void operator			-= (const Vector &v);				// operator -=
		void operator			*= (float f);						// scale Vector4
		void operator			/= (float f);						// scale down
		void operator			+= (float f);						// add scalar
		void operator			-= (float f);						// subtract scalar
		float  operator			* (const Vector &v) const;			// dot product
		Vector operator			* (float f)const;					// scale Vector4
		Vector operator			/ (float f)const;					// scalar divide
		Vector operator			+ (float f)const;					// add scalar
		Vector operator			- (float f)const;					// scale down
		Vector operator			+ (const Vector &v) const;			// addition
		Vector operator			- (const Vector &v) const;			// subtraction
	};
}

#if !defined(Export_SmtMathLib)
#if   defined( _DEBUG)
#          pragma comment(lib,"SmtMathLibD.lib")
#       else
#          pragma comment(lib,"SmtMathLib.lib")
#	    endif  
#endif

#endif //_SMT_MATH_LIB_H