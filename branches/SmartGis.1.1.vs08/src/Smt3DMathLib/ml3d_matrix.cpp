#include "ml3d_mathlib.h"
namespace Smt_3DMath
{

	inline void Matrix::Identity(void) 
	{
	   float *f = (float*)&this->_11;
	   memset(f, 0, sizeof(Matrix));
	   _11 = _22 = _33 = _44 = 1.0f;
	}

	// Build rotation matrix around X axis
	inline void Matrix::RotaX(float a) 
	{
	   float fCos = cosf(a);
	   float fSin = sinf(a);

	   _22 =  fCos;
	   _23 =  fSin;
	   _32 = -fSin;
	   _33 =  fCos;

	   _11 = _44 = 1.0f;
	   _12 = _13 = _14 = _21 = _24 = _31 = _34 = _41 = _42 = _43 = 0.0f;
	}

	// Build rotation matrix around Y axis
	inline void Matrix::RotaY(float a) 
	{
	   float fCos = cosf(a);
	   float fSin = sinf(a);

	   _11 =  fCos;
	   _13 = -fSin;
	   _31 =  fSin;
	   _33 =  fCos;

	   _22 = _44 = 1.0f;
	   _12 = _23 = _14 = _21 = _24 = _32 = _34 = _41 = _42 = _43 = 0.0f;
	}

	// Build rotation matrix around Z axis
	inline void Matrix::RotaZ(float a) 
	{
	   float fCos = cosf(a);
	   float fSin = sinf(a);

	   _11  =  fCos;
	   _12  =  fSin;
	   _21  = -fSin;
	   _22  =  fCos;

	   _33 = _44 = 1.0f;
	   _13 = _14 = _23 = _24 = _31 = _32 = _34 = _41 = _42 = _43 = 0.0f;
	}

	inline void Matrix::ApplyInverseRota(Vector4 *pvc) 
	{
	   pvc->x = pvc->x * _11 + pvc->y * _12 + pvc->z * _13;
	   pvc->y = pvc->x * _21 + pvc->y * _22 + pvc->z * _23;
	   pvc->z = pvc->x * _31 + pvc->y * _32 + pvc->z * _33;
	   pvc->w = 1.0f;
	}

	inline void Matrix::Rota(float x, float y, float z)
	{ 
		Rota(Vector4(x, y, z)); 
	}

	inline void Matrix::Rota(const Vector4 &vc) 
	{
	   float sr, sp, sy, cr, cp, cy;

	   Identity();

	   sy = sinf( vc.z );
	   cy = cosf( vc.z );
	   sp = sinf( vc.y );
	   cp = cosf( vc.y );
	   sr = sinf( vc.x );
	   cr = cosf( vc.x );
 
	   _11 = cp*cy;
	   _12 = cp*sy;
	   _13 = -sp;
	   _21 = sr*sp*cy+cr*-sy;
	   _22 = sr*sp*sy+cr*cy;
	   _23 = sr*cp;
	   _31 = (cr*sp*cy+-sr*-sy);
	   _32 = (cr*sp*sy+-sr*cy);
	   _33 = cr*cp;
	} // Rota

	inline void Matrix::SetTranslation(Vector4 vc, bool b) 
	{
	   if (b) 
		   Identity();

	   _41 = vc.x;
	   _42 = vc.y;
	   _43 = vc.z;
	} // SetTranslation

	inline Vector4 Matrix::GetTranslation(void)
	{ 
		return Vector4(_41, _42, _43); 
	}

	// Build rotation matrix around arbitrary axis
	inline void Matrix::RotaArbi(const Vector4 &_vcAxis, float a) 
	{
	   Vector4 vcAxis = _vcAxis;
	   float fCos = cosf(a);
	   float fSin = sinf(a);
	   float fSum = 1.0f - fCos;
   
	   if (vcAxis.GetSqrLength() != 1.0)
		  vcAxis.Normalize();

	   _11 = (vcAxis.x * vcAxis.x) * fSum + fCos;
	   _12 = (vcAxis.x * vcAxis.y) * fSum - (vcAxis.z * fSin);
	   _13 = (vcAxis.x * vcAxis.z) * fSum + (vcAxis.y * fSin);

	   _21 = (vcAxis.y * vcAxis.x) * fSum + (vcAxis.z * fSin);
	   _22 = (vcAxis.y * vcAxis.y) * fSum + fCos ;
	   _23 = (vcAxis.y * vcAxis.z) * fSum - (vcAxis.x * fSin);

	   _31 = (vcAxis.z * vcAxis.x) * fSum - (vcAxis.y * fSin);
	   _32 = (vcAxis.z * vcAxis.y) * fSum + (vcAxis.x * fSin);
	   _33 = (vcAxis.z * vcAxis.z) * fSum + fCos;

	   _14 = _24 = _34 = _41 = _42 = _43 = 0.0f;
	   _44 = 1.0f;
	}

	// add translation to matrix
	inline void Matrix::Translate(float dx, float dy, float dz) 
	{
	   _41 = dx;
	   _42 = dy;
	   _43 = dz;
	}

	inline void Matrix::Rotate(float angle,float x, float y, float z)
	{
		Vector4 vec(x,y,z);
		RotaArbi(vec,angle);
	}

	inline void Matrix::Scale(float sx, float sy, float sz)
	{
		_11 = sx;
		_22 = sy;
		_33 = sz;
	}

	void Matrix::SetPerspective( float fovy, float aspect, float zNear, float zFar)
	{
		/*float c,s,Q;

		c= (float) cos( 0.5f*DEG2RAD(fovy) );
		s= (float) sin( 0.5f*DEG2RAD(fovy) );

		Q= s/(1.0f-zNear/zFar);

		_11= c/(aspect*Q*zNear);
		_21= 0;
		_31= 0;
		_41= 0;
		_12= 0;
		_22= c/(Q*zNear);
		_32= 0;
		_42= 0;
		_13= 0;
		_23= 0;
		_33= -1/zNear;
		_43= -s/(Q*zNear);
		_14= 0;
		_24= 0;
		_34= -1;
		_44= 0;*/

		if(fabs(zFar - zNear) < 0.01f)
			return ;

		float sinFOV2 = sinf(0.5f*DEG2RAD(fovy));

		if(fabs(sinFOV2) < 0.01f)
			return ;

		float cosFOV2 = cosf(0.5f*DEG2RAD(fovy));

		float w = aspect * (cosFOV2 / sinFOV2);
		float h = 1.0f  * (cosFOV2 / sinFOV2);
		float Q = zFar / (zFar - zNear);

		_11 = w;
		_22 = h;
		_33 = Q;
		_43 = -Q*zNear;
		_34 = 1.0f;
		/*_34 = 1.0f;
		_43 = -Q*zNear;*/
	}

	// look from given position at given point
	inline void Matrix::LookAt(Vector4 vcPos, Vector4 vcLookAt,Vector4 vcWorldUp)
	{
	   Vector4 vcDir = vcLookAt - vcPos;
	   Vector4 vcUp, vcRight; 
	   float     fAngle=0.0f;

	   vcDir.Normalize();

	   fAngle = vcWorldUp * vcDir;

	   vcUp = vcWorldUp - (vcDir * fAngle);
	   vcUp.Normalize();

	   vcRight = vcUp.CrossProduct(vcDir);

	   _11 = vcRight.x; _21 = vcUp.x; _31 = vcDir.x;
	   _12 = vcRight.y; _22 = vcUp.y; _32 = vcDir.y;
	   _13 = vcRight.z; _23 = vcUp.z; _33 = vcDir.z;

	   _41 = vcPos.x;
	   _42 = vcPos.y;
	   _43 = vcPos.z;
   
	   _14=0.0f; _24=0.0f; _34=0.0f; _44=1.0f;
	} // LookAt

	// make a billboard matrix for given position and direction
	inline void Matrix::Billboard(Vector4 vcPos, Vector4 vcDir,Vector4 vcWorldUp) 
	{
	   Vector4 vcUp, vcRight; 
	   float     fAngle=0.0f;

	   fAngle = vcWorldUp * vcDir;

	   vcUp = vcWorldUp - (vcDir * fAngle);
	   vcUp.Normalize();

	   vcRight = vcUp.CrossProduct(vcDir);

	   _11 = vcRight.x; _21 = vcUp.x; _31 = vcDir.x;
	   _12 = vcRight.y; _22 = vcUp.y; _32 = vcDir.y;
	   _13 = vcRight.z; _23 = vcUp.z; _33 = vcDir.z;

	   _41 = vcPos.x;
	   _42 = vcPos.y;
	   _43 = vcPos.z;
   
	   _14=0.0f; _24=0.0f; _34=0.0f; _44=1.0f;
	 } // Billboard

	// multiply two matrices
	Matrix Matrix::operator * (const Matrix &m) const 
	{
	   Matrix mResult;

		  float *pA = (float*)this;
		  float *pB = (float*)&m;
		  float *pM = (float*)&mResult;

		  memset(pM, 0, sizeof(Matrix));

		  for(unsigned char i=0; i<4; i++) 
			 for(unsigned char j=0; j<4; j++) 
			 {
				pM[4*i+j] += pA[4*i]   * pB[j];
				pM[4*i+j] += pA[4*i+1] * pB[4+j];
				pM[4*i+j] += pA[4*i+2] * pB[8+j];
				pM[4*i+j] += pA[4*i+3] * pB[12+j];
			 }
	   return mResult;
	 } 

	void Matrix::operator*= (const Matrix &m)
	{
		Matrix mResult = (*this)*m;
		memcpy(this,&mResult,sizeof(float)*16);
	}

	// multiply matrix with a Vector4
	Vector4 Matrix::operator * (const Vector4 &vc) const 
	{
	   Vector4 vcResult;

		  vcResult.x = vc.x*_11 + vc.y*_21 + vc.z*_31 + _41;
		  vcResult.y = vc.x*_12 + vc.y*_22 + vc.z*_32 + _42;
		  vcResult.z = vc.x*_13 + vc.y*_23 + vc.z*_33 + _43;
		  vcResult.w = vc.x*_14 + vc.y*_24 + vc.z*_34 + _44;
   
		  vcResult.x = vcResult.x/vcResult.w;
		  vcResult.y = vcResult.y/vcResult.w;
		  vcResult.z = vcResult.z/vcResult.w;
		  vcResult.w = 1.0f;
  
	   return vcResult;
	}

	// transpose the matrix
	inline void Matrix::TransposeOf(const Matrix &m) 
	{
	   _11 = m._11;
	   _21 = m._12;
	   _31 = m._13;
	   _41 = m._14;

	   _12 = m._21;
	   _22 = m._22;
	   _32 = m._23;
	   _42 = m._24;

	   _13 = m._31;
	   _23 = m._32;
	   _33 = m._33;
	   _43 = m._34;

	   _14 = m._41;
	   _24 = m._42;
	   _34 = m._43;
	   _44 = m._44;
	   }

	// invert the matrix, use of intel's SSE code is incredibly slow here.
	inline void Matrix::InverseOf(const Matrix &m) 
	{
 
		  Matrix mTrans;
		  float     fTemp[12],  // cofaktors
					fDet;

		  // calculate transposed matrix
		  mTrans.TransposeOf(m);

		  // Paare für die ersten 8 Kofaktoren
		  fTemp[ 0]  = mTrans._33 * mTrans._44;
		  fTemp[ 1]  = mTrans._34 * mTrans._43;
		  fTemp[ 2]  = mTrans._32 * mTrans._44;
		  fTemp[ 3]  = mTrans._34 * mTrans._42;
		  fTemp[ 4]  = mTrans._32 * mTrans._43;
		  fTemp[ 5]  = mTrans._33 * mTrans._42;
		  fTemp[ 6]  = mTrans._31 * mTrans._44;
		  fTemp[ 7]  = mTrans._34 * mTrans._41;
		  fTemp[ 8]  = mTrans._31 * mTrans._43;
		  fTemp[ 9]  = mTrans._33 * mTrans._41;
		  fTemp[10]  = mTrans._31 * mTrans._42;
		  fTemp[11]  = mTrans._32 * mTrans._41;

		  // Berechne die ersten 8 Kofaktoren
		  this->_11  = fTemp[0]*mTrans._22 + fTemp[3]*mTrans._23 + fTemp[4] *mTrans._24;
		  this->_11 -= fTemp[1]*mTrans._22 + fTemp[2]*mTrans._23 + fTemp[5] *mTrans._24;
		  this->_12  = fTemp[1]*mTrans._21 + fTemp[6]*mTrans._23 + fTemp[9] *mTrans._24;
		  this->_12 -= fTemp[0]*mTrans._21 + fTemp[7]*mTrans._23 + fTemp[8] *mTrans._24;
		  this->_13  = fTemp[2]*mTrans._21 + fTemp[7]*mTrans._22 + fTemp[10]*mTrans._24;
		  this->_13 -= fTemp[3]*mTrans._21 + fTemp[6]*mTrans._22 + fTemp[11]*mTrans._24;
		  this->_14  = fTemp[5]*mTrans._21 + fTemp[8]*mTrans._22 + fTemp[11]*mTrans._23;
		  this->_14 -= fTemp[4]*mTrans._21 + fTemp[9]*mTrans._22 + fTemp[10]*mTrans._23;
		  this->_21  = fTemp[1]*mTrans._12 + fTemp[2]*mTrans._13 + fTemp[5] *mTrans._14;
		  this->_21 -= fTemp[0]*mTrans._12 + fTemp[3]*mTrans._13 + fTemp[4] *mTrans._14;
		  this->_22  = fTemp[0]*mTrans._11 + fTemp[7]*mTrans._13 + fTemp[8] *mTrans._14;
		  this->_22 -= fTemp[1]*mTrans._11 + fTemp[6]*mTrans._13 + fTemp[9] *mTrans._14;
		  this->_23  = fTemp[3]*mTrans._11 + fTemp[6]*mTrans._12 + fTemp[11]*mTrans._14;
		  this->_23 -= fTemp[2]*mTrans._11 + fTemp[7]*mTrans._12 + fTemp[10]*mTrans._14;
		  this->_24  = fTemp[4]*mTrans._11 + fTemp[9]*mTrans._12 + fTemp[10]*mTrans._13;
		  this->_24 -= fTemp[5]*mTrans._11 + fTemp[8]*mTrans._12 + fTemp[11]*mTrans._13;

		  // Paare f die zweiten 8 Kofaktoren
		  fTemp[ 0]  = mTrans._13 * mTrans._24;
		  fTemp[ 1]  = mTrans._14 * mTrans._23;
		  fTemp[ 2]  = mTrans._12 * mTrans._24;
		  fTemp[ 3]  = mTrans._14 * mTrans._22;
		  fTemp[ 4]  = mTrans._12 * mTrans._23;
		  fTemp[ 5]  = mTrans._13 * mTrans._22;
		  fTemp[ 6]  = mTrans._11 * mTrans._24;
		  fTemp[ 7]  = mTrans._14 * mTrans._21;
		  fTemp[ 8]  = mTrans._11 * mTrans._23;
		  fTemp[ 9]  = mTrans._13 * mTrans._21;
		  fTemp[10]  = mTrans._11 * mTrans._22;
		  fTemp[11]  = mTrans._12 * mTrans._21;

		  // Berechne die zweiten 8 Kofaktoren
		  this->_31  = fTemp[0] *mTrans._42 + fTemp[3] *mTrans._43 + fTemp[4] *mTrans._44;
		  this->_31 -= fTemp[1] *mTrans._42 + fTemp[2] *mTrans._43 + fTemp[5] *mTrans._44;
		  this->_32  = fTemp[1] *mTrans._41 + fTemp[6] *mTrans._43 + fTemp[9] *mTrans._44;
		  this->_32 -= fTemp[0] *mTrans._41 + fTemp[7] *mTrans._43 + fTemp[8] *mTrans._44;
		  this->_33  = fTemp[2] *mTrans._41 + fTemp[7] *mTrans._42 + fTemp[10]*mTrans._44;
		  this->_33 -= fTemp[3] *mTrans._41 + fTemp[6] *mTrans._42 + fTemp[11]*mTrans._44;
		  this->_34  = fTemp[5] *mTrans._41 + fTemp[8] *mTrans._42 + fTemp[11]*mTrans._43;
		  this->_34 -= fTemp[4] *mTrans._41 + fTemp[9] *mTrans._42 + fTemp[10]*mTrans._43;
		  this->_41  = fTemp[2] *mTrans._33 + fTemp[5] *mTrans._34 + fTemp[1] *mTrans._32;
		  this->_41 -= fTemp[4] *mTrans._34 + fTemp[0] *mTrans._32 + fTemp[3] *mTrans._33;
		  this->_42  = fTemp[8] *mTrans._34 + fTemp[0] *mTrans._31 + fTemp[7] *mTrans._33;
		  this->_42 -= fTemp[6] *mTrans._33 + fTemp[9] *mTrans._34 + fTemp[1] *mTrans._31;
		  this->_43  = fTemp[6] *mTrans._32 + fTemp[11]*mTrans._34 + fTemp[3] *mTrans._31;
		  this->_43 -= fTemp[10]*mTrans._34 + fTemp[2] *mTrans._31 + fTemp[7] *mTrans._32;
		  this->_44  = fTemp[10]*mTrans._33 + fTemp[4] *mTrans._31 + fTemp[9] *mTrans._32;
		  this->_44 -= fTemp[8] *mTrans._32 + fTemp[11]*mTrans._33 + fTemp[5] *mTrans._31;

		  fDet = mTrans._11*this->_11 + 
				 mTrans._12*this->_12 + 
				 mTrans._13*this->_13 +
				 mTrans._14*this->_14;

		  fDet = 1/fDet;

		  this->_11 *= fDet;  
		  this->_12 *= fDet;  
		  this->_13 *= fDet;  
		  this->_14 *= fDet;

		  this->_21 *= fDet;  
		  this->_22 *= fDet;  
		  this->_23 *= fDet;  
		  this->_24 *= fDet;

		  this->_31 *= fDet;  
		  this->_32 *= fDet;  
		  this->_33 *= fDet;  
		  this->_34 *= fDet;

		  this->_41 *= fDet;  
		  this->_42 *= fDet;  
		  this->_43 *= fDet;  
		  this->_44 *= fDet;
	}
}