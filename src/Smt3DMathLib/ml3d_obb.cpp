#include "ml3d_mathlib.h"
namespace Smt_3DMath
{

	inline void Obb::DeTransform(const Obb &obb, const Matrix &m) 
	{
	   Matrix mat = m;
	   Vector4 vcT;
   
	   // erase translation from mat
	   vcT.Set(mat._41, mat._42, mat._43);
	   mat._41 = mat._42 = mat._43 = 0.0f;

	   // rotate center and axis to matrix coord.-space
	   this->vcCenter = mat * obb.vcCenter;
	   this->vcA0     = mat * obb.vcA0;
	   this->vcA1     = mat * obb.vcA1;
	   this->vcA2     = mat * obb.vcA2;

	   // set translation
	   this->vcCenter += vcT;

	   // copy axis length
	   fA0 = obb.fA0;
	   fA1 = obb.fA1;
	   fA2 = obb.fA2;
	} // Transform
 
	// helperfunction
	void Obb::ObbProj(const Obb &Obb, const Vector4 &vcV,float *pfMin, float *pfMax) 
	{
	   float fDP = vcV * Obb.vcCenter;
	   float fR = Obb.fA0 * _fabs(vcV * Obb.vcA0) +
				  Obb.fA0 * _fabs(vcV * Obb.vcA1) +
				  Obb.fA1 * _fabs(vcV * Obb.vcA2);
	   *pfMin = fDP - fR;
	   *pfMax = fDP + fR;
	} // ObbProj
 

	// helperfunction
	void Obb::TriProj(const Vector4 &v0, const Vector4 &v1, const Vector4 &v2, const Vector4 &vcV,float *pfMin, float *pfMax) 
	{
	   *pfMin = vcV * v0;
	   *pfMax = *pfMin;
   
	   float fDP = vcV * v1;
	   if (fDP < *pfMin) *pfMin = fDP;
	   else if (fDP > *pfMax) *pfMax = fDP;
   
	   fDP = vcV * v2;
	   if (fDP < *pfMin) *pfMin = fDP;
	   else if (fDP > *pfMax) *pfMax = fDP;
	} // TriProj
 

	// intersects trianlge
	bool Obb::Intersects(const Vector4 &v0, const Vector4 &v1, const Vector4 &v2) 
	{
	   float     fMin0, fMax0, fMin1, fMax1;
	   float     fD_C;
	   Vector4 vcV, vcTriEdge[3], vcA[3];
   
	   // just for looping
	   vcA[0] = this->vcA0;
	   vcA[1] = this->vcA1;
	   vcA[2] = this->vcA2;

	   // direction of tri-normals
	   vcTriEdge[0] = v1 - v0;
	   vcTriEdge[1] = v2 - v0;

	   vcV = vcTriEdge[0].CrossProduct(vcTriEdge[1]);

	   fMin0 = vcV * v0;
	   fMax0 = fMin0;

	   this->ObbProj((*this), vcV, &fMin1, &fMax1);
	   if ( fMax1 < fMin0 || fMax0 < fMin1 )
		  return true;
   
	   // direction of obb planes
	   // =======================
	   // axis 1:
	   vcV = this->vcA0;
	   this->TriProj(v0, v1, v2, vcV, &fMin0, &fMax0);
	   fD_C = vcV * this->vcCenter;
	   fMin1 = fD_C - this->fA0;
	   fMax1 = fD_C + this->fA0;
	   if ( fMax1 < fMin0 || fMax0 < fMin1 )
		  return true;
   
	   // axis 2:
	   vcV = this->vcA1;
	   this->TriProj(v0, v1, v2, vcV, &fMin0, &fMax0);
	   fD_C = vcV * this->vcCenter;
	   fMin1 = fD_C - this->fA1;
	   fMax1 = fD_C + this->fA1;
	   if ( fMax1 < fMin0 || fMax0 < fMin1 )
		  return true;

	   // axis 3:
	   vcV = this->vcA2;
	   this->TriProj(v0, v1, v2, vcV, &fMin0, &fMax0);
	   fD_C = vcV * this->vcCenter;
	   fMin1 = fD_C - this->fA2;
	   fMax1 = fD_C + this->fA2;
	   if ( fMax1 < fMin0 || fMax0 < fMin1 )
		  return true;


	   // direction of tri-obb edge-crossproducts
	   vcTriEdge[2] = vcTriEdge[1] - vcTriEdge[0];
	   for (int j=0; j<3; j++) 
	   {
		  for (int k=0; k<3; k++) 
		  {
			 vcV = vcTriEdge[j].CrossProduct(vcA[k]);

			 this->TriProj(v0, v1, v2, vcV, &fMin0, &fMax0);
			 this->ObbProj((*this), vcV, &fMin1, &fMax1);
         
			 if ( (fMax1 < fMin0) || (fMax0 < fMin1) )
				return true;
		  }
	   }
   
	   return true;
	} // Intersects(Tri)
 
	// intersects ray, slaps method
	bool Obb::Intersects(const Ray &ray, float *t) 
	{
	   float e, f, t1, t2, temp;
	   float tmin = -99999.9f, 
			 tmax = +99999.9f;

	   Vector4 vcP = this->vcCenter - ray.m_vcOrig;

	   // 1st slap
	   e = this->vcA0 * vcP;
	   f = this->vcA0 * ray.m_vcDir;
	   if (_fabs(f) > 0.00001f) 
	   {

		  t1 = (e + this->fA0) / f;
		  t2 = (e - this->fA0) / f;

		  if (t1 > t2) { temp=t1; t1=t2; t2=temp; }
		  if (t1 > tmin) tmin = t1;
		  if (t2 < tmax) tmax = t2;
		  if (tmin > tmax) return false;
		  if (tmax < 0.0f) return false;
		  }
	   else if ( ((-e - this->fA0) > 0.0f) || ((-e + this->fA0) < 0.0f) )
		  return false;

	   // 2nd slap
	   e = this->vcA1 * vcP;
	   f = this->vcA1 * ray.m_vcDir;
	   if (_fabs(f) > 0.00001f) 
	   {

		  t1 = (e + this->fA1) / f;
		  t2 = (e - this->fA1) / f;

		  if (t1 > t2) { temp=t1; t1=t2; t2=temp; }
		  if (t1 > tmin) tmin = t1;
		  if (t2 < tmax) tmax = t2;
		  if (tmin > tmax) return false;
		  if (tmax < 0.0f) return false;
	   }
	   else if ( ((-e - this->fA1) > 0.0f) || ((-e + this->fA1) < 0.0f) )
		  return false;

	   // 3rd slap
	   e = this->vcA2 * vcP;
	   f = this->vcA2 * ray.m_vcDir;
	   if (_fabs(f) > 0.00001f) 
	   {

		  t1 = (e + this->fA2) / f;
		  t2 = (e - this->fA2) / f;

		  if (t1 > t2) { temp=t1; t1=t2; t2=temp; }
		  if (t1 > tmin) tmin = t1;
		  if (t2 < tmax) tmax = t2;
		  if (tmin > tmax) return false;
		  if (tmax < 0.0f) return false;
	   }
	   else if ( ((-e - this->fA2) > 0.0f) || ((-e + this->fA2) < 0.0f) )
		  return false;

	   if (tmin > 0.0f) 
	   {
		  if (t) *t = tmin;
		  return true;
	   }

	   if (t) *t = tmax;

	   return true;
	} // Intersects(Ray)
 
	// intersects ray at certain length (line segment), slaps method
	bool Obb::Intersects(const Ray &ray, float fL, float *t) 
	{
	   float e, f, t1, t2, temp;
	   float tmin = -99999.9f, 
			 tmax = +99999.9f;

	   Vector4 vcP = this->vcCenter - ray.m_vcOrig;

	   // 1st slap
	   e = this->vcA0 * vcP;
	   f = this->vcA0 * ray.m_vcDir;
	   if (_fabs(f) > 0.00001f) 
	   {

		  t1 = (e + this->fA0) / f;
		  t2 = (e - this->fA0) / f;

		  if (t1 > t2) 
		  { 
			  temp=t1; 
			  t1=t2; 
			  t2=temp; 
		  }
		  if (t1 > tmin) tmin = t1;
		  if (t2 < tmax) tmax = t2;
		  if (tmin > tmax) return false;
		  if (tmax < 0.0f) return false;
	   }
	   else if ( ((-e - this->fA0) > 0.0f) || ((-e + this->fA0) < 0.0f) )
		  return false;

	   // 2nd slap
	   e = this->vcA1 * vcP;
	   f = this->vcA1 * ray.m_vcDir;
	   if (_fabs(f) > 0.00001f) 
	   {

		  t1 = (e + this->fA1) / f;
		  t2 = (e - this->fA1) / f;

		  if (t1 > t2) { temp=t1; t1=t2; t2=temp; }
		  if (t1 > tmin) tmin = t1;
		  if (t2 < tmax) tmax = t2;
		  if (tmin > tmax) return false;
		  if (tmax < 0.0f) return false;
	   }
	   else if ( ((-e - this->fA1) > 0.0f) || ((-e + this->fA1) < 0.0f) )
		  return false;

	   // 3rd slap
	   e = this->vcA2 * vcP;
	   f = this->vcA2 * ray.m_vcDir;
	   if (_fabs(f) > 0.00001f) 
	   {

		  t1 = (e + this->fA2) / f;
		  t2 = (e - this->fA2) / f;

		  if (t1 > t2) { temp=t1; t1=t2; t2=temp; }
		  if (t1 > tmin) tmin = t1;
		  if (t2 < tmax) tmax = t2;
		  if (tmin > tmax) return false;
		  if (tmax < 0.0f) return false;
	   }
	   else if ( ((-e - this->fA2) > 0.0f) || ((-e + this->fA2) < 0.0f) )
		  return false;

	   if ( (tmin > 0.0f) && (tmin <= fL) ) 
	   {
		  if (t) *t = tmin;
		  return true;
	   }

	   // intersection on line but not on segment
	   if (tmax > fL) return false;

	   if (t) *t = tmax;

	   return true;
	} // Intersects(Segment)
 
	// intersects another obb
	bool Obb::Intersects(const Obb &obb) 
	{
	   float T[3];
   
	   // difference Vector4 between both obb
	   Vector4 vcD = obb.vcCenter - this->vcCenter;

	   float matM[3][3];   // B's axis in relation to A
	   float ra,           // radius A
			 rb,           // radius B
			 t;            // absolute values from T[]
   
	   // Obb A's axis as separation axis?
	   // ================================
	   // first axis vcA0
	   matM[0][0] = this->vcA0 * obb.vcA0;
	   matM[0][1] = this->vcA0 * obb.vcA1;
	   matM[0][2] = this->vcA0 * obb.vcA2;
	   ra   = this->fA0;
	   rb   = obb.fA0 * _fabs(matM[0][0]) + 
			  obb.fA1 * _fabs(matM[0][1]) + 
			  obb.fA2 * _fabs(matM[0][2]);

	   T[0] = vcD * this->vcA0;
	   t    = _fabs(T[0]);
	   if(t > (ra + rb) ) 
		  return false;

	   // second axis vcA1
	   matM[1][0] = this->vcA1 * obb.vcA0;
	   matM[1][1] = this->vcA1 * obb.vcA1;
	   matM[1][2] = this->vcA1 * obb.vcA2;
	   ra   = this->fA1;
	   rb   = obb.fA0 * _fabs(matM[1][0]) + 
			  obb.fA1 * _fabs(matM[1][1]) + 
			  obb.fA2 * _fabs(matM[1][2]);
	   T[1] = vcD * this->vcA1;
	   t    = _fabs(T[1]);
	   if(t > (ra + rb) ) 
		  return false;

	   // third axis vcA2
	   matM[2][0] = this->vcA2 * obb.vcA0;
	   matM[2][1] = this->vcA2 * obb.vcA1;
	   matM[2][2] = this->vcA2 * obb.vcA2;
	   ra   = this->fA2;
	   rb   = obb.fA0 * _fabs(matM[2][0]) + 
			  obb.fA1 * _fabs(matM[2][1]) + 
			  obb.fA2 * _fabs(matM[2][2]);
	   T[2] = vcD * this->vcA2;
	   t    = _fabs(T[2]);
	   if(t > (ra + rb) ) 
		  return false;

	   // Obb B's axis as separation axis?
	   // ================================
	   // first axis vcA0
	   ra = this->fA0 * _fabs(matM[0][0]) + 
			this->fA1 * _fabs(matM[1][0]) + 
			this->fA2 * _fabs(matM[2][0]);
	   rb = obb.fA0;
	   t = _fabs( T[0]*matM[0][0] + T[1]*matM[1][0] + T[2]*matM[2][0] );
	   if(t > (ra + rb) )
		  return false;

	   // second axis vcA1
	   ra = this->fA0 * _fabs(matM[0][1]) + 
			this->fA1 * _fabs(matM[1][1]) + 
			this->fA2 * _fabs(matM[2][1]);
	   rb = obb.fA1;
	   t = _fabs( T[0]*matM[0][1] + T[1]*matM[1][1] + T[2]*matM[2][1] );
	   if(t > (ra + rb) )
		  return false;

	   // third axis vcA2
	   ra = this->fA0 * _fabs(matM[0][2]) + 
			this->fA1 * _fabs(matM[1][2]) + 
			this->fA2 * _fabs(matM[2][2]);
	   rb = obb.fA2;
	   t = _fabs( T[0]*matM[0][2] + T[1]*matM[1][2] + T[2]*matM[2][2] );
	   if(t > (ra + rb) )
		  return false;

	   // other candidates: cross products of axis:
	   // =========================================
	   // axis A0xB0
	   ra = this->fA1*_fabs(matM[2][0]) + this->fA2*_fabs(matM[1][0]);
	   rb = obb.fA1*_fabs(matM[0][2]) + obb.fA2*_fabs(matM[0][1]);
	   t = _fabs( T[2]*matM[1][0] - T[1]*matM[2][0] );
	   if( t > ra + rb )
		  return false;
   
	   // axis A0xB1
	   ra = this->fA1*_fabs(matM[2][1]) + this->fA2*_fabs(matM[1][1]);
	   rb = obb.fA0*_fabs(matM[0][2]) + obb.fA2*_fabs(matM[0][0]);
	   t = _fabs( T[2]*matM[1][1] - T[1]*matM[2][1] );
	   if( t > ra + rb )
		  return false;
   
	   // axis A0xB2
	   ra = this->fA1*_fabs(matM[2][2]) + this->fA2*_fabs(matM[1][2]);
	   rb = obb.fA0*_fabs(matM[0][1]) + obb.fA1*_fabs(matM[0][0]);
	   t = _fabs( T[2]*matM[1][2] - T[1]*matM[2][2] );
	   if( t > ra + rb )
		  return false;
   
	   // axis A1xB0
	   ra = this->fA0*_fabs(matM[2][0]) + this->fA2*_fabs(matM[0][0]);
	   rb = obb.fA1*_fabs(matM[1][2]) + obb.fA2*_fabs(matM[1][1]);
	   t = _fabs( T[0]*matM[2][0] - T[2]*matM[0][0] );
	   if( t > ra + rb )
		  return false;
   
	   // axis A1xB1
	   ra = this->fA0*_fabs(matM[2][1]) + this->fA2*_fabs(matM[0][1]);
	   rb = obb.fA0*_fabs(matM[1][2]) + obb.fA2*_fabs(matM[1][0]);
	   t = _fabs( T[0]*matM[2][1] - T[2]*matM[0][1] );
	   if( t > ra + rb )
		  return false;
   
	   // axis A1xB2
	   ra = this->fA0*_fabs(matM[2][2]) + this->fA2*_fabs(matM[0][2]);
	   rb = obb.fA0*_fabs(matM[1][1]) + obb.fA1*_fabs(matM[1][0]);
	   t = _fabs( T[0]*matM[2][2] - T[2]*matM[0][2] );
	   if( t > ra + rb )
		  return false;
   
	   // axis A2xB0
	   ra = this->fA0*_fabs(matM[1][0]) + this->fA1*_fabs(matM[0][0]);
	   rb = obb.fA1*_fabs(matM[2][2]) + obb.fA2*_fabs(matM[2][1]);
	   t = _fabs( T[1]*matM[0][0] - T[0]*matM[1][0] );
	   if( t > ra + rb )
		  return false;
   
	   // axis A2xB1
	   ra = this->fA0*_fabs(matM[1][1]) + this->fA1*_fabs(matM[0][1]);
	   rb = obb.fA0 *_fabs(matM[2][2]) + obb.fA2*_fabs(matM[2][0]);
	   t = _fabs( T[1]*matM[0][1] - T[0]*matM[1][1] );
	   if( t > ra + rb )
		  return false;
   
	   // axis A2xB2
	   ra = this->fA0*_fabs(matM[1][2]) + this->fA1*_fabs(matM[0][2]);
	   rb = obb.fA0*_fabs(matM[2][1]) + obb.fA1*_fabs(matM[2][0]);
	   t = _fabs( T[1]*matM[0][2] - T[0]*matM[1][2] );
	   if( t > ra + rb )
		  return false;
   
	   // no separation axis found => intersection
	   return true;
	} // Intersects(obb)
 
	/**
	 * Culls OBB to n sided frustrum. Normals pointing outwards.
	 * -> IN:  Plane   - array of planes building frustrum
	 *         int        - number of planes in array
	 *    OUT: VISIBLE - obb totally inside frustrum
	 *         CLIPPED - obb clipped by frustrum
	 *         CULLED  - obb totally outside frustrum
	 */
	int Obb::Cull(const Plane *pPlanes, int nNumPlanes)
	{
	   Vector4 vN;
	   int       nResult = VISIBLE;
	   float     fRadius, fTest;

	   // for all planes
	   for (int i=0; i<nNumPlanes; i++) 
	   {
		  // frustrum normals pointing outwards, we need inwards
		  vN = pPlanes[i].m_vcN * -1.0f;

		  // calculate projected box radius
		  fRadius = _fabs(fA0 * (vN * vcA0)) 
				  + _fabs(fA1 * (vN * vcA1))
				  + _fabs(fA2 * (vN * vcA2));

		  // testvalue: (N*C - d) (#)
		  fTest = vN * this->vcCenter - pPlanes[i].m_fD;

		  // obb totally outside of at least one plane: (#) < -r
		  if (fTest < -fRadius)
			 return CULLED;
		  // or maybe intersecting this plane?
		  else if (!(fTest > fRadius))
			 nResult = CLIPPED;
		} // for

	   // if not culled then clipped or inside
	   return nResult;
	}
 
}