#include "ml3d_mathlib.h"
namespace Smt_3DMath
{
	Polygon::Polygon(void) 
	{
	   m_pPoints = NULL;
	   m_pIndis  = NULL;
	   m_NumP    = 0;
	   m_NumI    = 0;
	   m_Flag    = 0;
	   memset(&m_Aabb, 0, sizeof(Aabb));
   
	} // constructor
 

	Polygon::~Polygon(void) 
	{
	   if (m_pPoints) 
	   {
		  delete [] m_pPoints;
		  m_pPoints = NULL;
	   }

	   if (m_pIndis) 
	   {
		  delete [] m_pIndis;
		  m_pIndis = NULL;
	   }
	} // destructor
 

	void Polygon::Set(const Vector4 *pPoints, int nNumP,const unsigned int *pIndis, int nNumI)
	{
	   Vector4 vcEdge0, 
				 vcEdge1;

	   bool bGotEm = false;

	   if (m_pPoints) 
	   {
		  delete [] m_pPoints;
		  m_pPoints = NULL;
	   }

	   if (m_pIndis) 
	   {
		  delete [] m_pIndis;
		  m_pIndis = NULL;
	   }

	   m_pPoints = new Vector4[nNumP];
	   m_pIndis  = new unsigned int[nNumI];

	   m_NumP = nNumP;
	   m_NumI = nNumI;

	   memcpy(m_pPoints, pPoints, sizeof(Vector4)*nNumP);
	   memcpy(m_pIndis,  pIndis,  sizeof(unsigned int)*nNumI);

	   vcEdge0 = m_pPoints[m_pIndis[1]] - m_pPoints[m_pIndis[0]];

	   // calculate its plane
	   for (int i=2; bGotEm==false; i++) {

		  if ((i+1) > m_NumI) break;

		  vcEdge1 = m_pPoints[m_pIndis[i]] - m_pPoints[m_pIndis[0]];

		  vcEdge0.Normalize();
		  vcEdge1.Normalize();

		  // edges must not be parallel
		  if (vcEdge0.AngleWith(vcEdge1) != 0.0)        
			 bGotEm = true;
		  } // for

	   m_Plane.m_vcN = vcEdge0.CrossProduct(vcEdge1);
	   m_Plane.m_vcN.Normalize();
	   m_Plane.m_fD = -(m_Plane.m_vcN * m_pPoints[0]);
	   m_Plane.m_vcPoint = m_pPoints[0];

	   CalcBoundingBox();
	} // Set
 
	void Polygon::CalcBoundingBox(void) 
	{
	   Vector4 vcMax, vcMin;

	   // pick random start value from poly
	   vcMax = vcMin = m_pPoints[0];

	   // seek final values for each coordinate
	   for (int i=0; i<m_NumP; i++) 
	   {
		  if ( m_pPoints[i].x > vcMax.x ) 
			 vcMax.x = m_pPoints[i].x;
		  else if ( m_pPoints[i].x < vcMin.x ) 
			 vcMin.x = m_pPoints[i].x;

		  if ( m_pPoints[i].y > vcMax.y ) 
			 vcMax.y = m_pPoints[i].y;
		  else if ( m_pPoints[i].y < vcMin.y ) 
			 vcMin.y = m_pPoints[i].y;

		  if ( m_pPoints[i].z > vcMax.z ) 
			 vcMax.z = m_pPoints[i].z;
		  else if ( m_pPoints[i].z < vcMin.z ) 
			 vcMin.z = m_pPoints[i].z;
		  } // for

	   m_Aabb.vcMax    = vcMax;
	   m_Aabb.vcMin    = vcMin;
	   m_Aabb.vcCenter = (vcMax + vcMin) / 2.0f;
	} // CalcBoundingBox
 

	// change frontside to backside and vice versa
	void Polygon::SwapFaces(void) 
	{
	   unsigned int *pIndis = new unsigned int[m_NumI];
   
	   // change index ordering
	   for (int i=0; i<m_NumI; i++)
		  pIndis[m_NumI-i-1] = m_pIndis[i];

	   // change normal orientation
	   m_Plane.m_vcN *= -1.0f;
	   m_Plane.m_fD  *= -1.0f;

	   delete [] m_pIndis;
	   m_pIndis = pIndis;
	} // SwapFaces
 

	// split this poly with the given plane, returns two new polys
	// on front and back side of the plane.
	void Polygon::Clip(const Plane &plane, Polygon *pFront, Polygon *pBack) 
	{
	   if (!pFront && !pBack) return;

	   Vector4 vcHit, vcA, vcB;
	   Ray    Ray;

	   // cast away const
	   Plane  *pPlane = ((Plane*)&plane);

	   int          nNumFront=0,     // number points on frontside
					nNumBack=0,      // number points on backside
					nLoop=0,
					nCurrent=0;
   
	   Vector4 *pvcFront = new Vector4[m_NumP*3];
	   Vector4 *pvcBack  = new Vector4[m_NumP*3];
   
	   // classify the first vertex and fill to appropriate list
	   switch (pPlane->Classify(m_pPoints[0])) 
	   {
		  case FRONT:
			 pvcFront[nNumFront++] = m_pPoints[0];
			 break;
		  case BACK:
			 pvcBack[nNumBack++] = m_pPoints[0];
			 break;
		  case PLANAR:
			 pvcBack[nNumBack++]   = m_pPoints[0];
			 pvcFront[nNumFront++] = m_pPoints[0];
			 break;
		  default: return;
		}
   
	   // loop through all points of the polygon
	   for (nLoop=1; nLoop < (m_NumP+1); nLoop++) 
	   {

		  if (nLoop == m_NumP) nCurrent = 0;
		  else nCurrent = nLoop;

		  // take two neighboring points from the poly
		  vcA = m_pPoints[nLoop-1];
		  vcB = m_pPoints[nCurrent];
      
		  // classify points with respect to the plane
		  int nClass  = pPlane->Classify(vcB);
		  int nClassA = pPlane->Classify(vcA);

		  // if planar then put him to both sides
		  if (nClass == PLANAR) 
		  {
			 pvcBack[nNumBack++]   = m_pPoints[nCurrent];
			 pvcFront[nNumFront++] = m_pPoints[nCurrent];
		  }
		  // else check if this edge intersects the plane
		  else 
		  {
			 Ray.m_vcOrig = vcA;
			 Ray.m_vcDir  = vcB - vcA;

			 float fLength = Ray.m_vcDir.GetLength();
        
			 if (fLength != 0.0f) Ray.m_vcDir /= fLength;

			 if ( Ray.Intersects(plane, false, fLength, 0, &vcHit)&& (nClassA != PLANAR)) 
			 {

				// put the intersection point as new point for both
				pvcBack[nNumBack++]   = vcHit;			
				pvcFront[nNumFront++] = vcHit;

				// then sort the current point
				if (nClass == FRONT) 
				{
				   if (nCurrent != 0) pvcFront[nNumFront++] = m_pPoints[nCurrent];
				} // if [FRONT]
				else if (nClass == BACK) 
				{
				   if (nCurrent != 0)pvcBack[nNumBack++] = m_pPoints[nCurrent];
				} // else [BACK]		
			 }
			 // if no intersection then just sort this point
			 else 
			 {
				if (nCurrent == 0) continue;

				if (nClass == FRONT) 
				{
				   pvcFront[nNumFront++] = m_pPoints[nCurrent];
				}
				else if (nClass == BACK) 
				{
				   pvcBack[nNumBack++] = m_pPoints[nCurrent];
				}
				else 
				{
				   return; // should not happen
				}
			  }
		   }
		} // for [NumP]


	   /* now we have the vertices for both new polys ready.
		  lets take care of the index lists now              */
   
	   unsigned int I0, I1, I2;

	   unsigned int *pnFront = NULL;
	   unsigned int *pnBack  = NULL;
   
	   if (nNumFront > 2) 
	   {
		  pnFront = new unsigned int[(nNumFront-2)*3];

		  for (nLoop=0; nLoop < (nNumFront-2); nLoop++) 
		  {
			 if (nLoop==0) 
			 { 
				 I0=0; 
				 I1=1; 
				 I2=2; 
			 }
			 else 
			 { 
				 I1=I2; 
				 I2++; 
			 }

			 pnFront[(nLoop*3)   ] = I0;
			 pnFront[(nLoop*3) +1] = I1;
			 pnFront[(nLoop*3) +2] = I2;
		  }
	   }

	   if (nNumBack > 2) 
	   {
		  pnBack  = new unsigned int[(nNumBack-2)*3];

		  for (nLoop=0; nLoop < (nNumBack-2); nLoop++) 
		  {
			 if (nLoop==0) { I0=0; I1=1; I2=2; }
			 else { I1=I2; I2++; }

			 pnBack[(nLoop*3)   ] = I0;
			 pnBack[(nLoop*3) +1] = I1;
			 pnBack[(nLoop*3) +2] = I2;
		  }
      
	   }

	   // now fuel in the information to the new poly objects
	   if (pFront && pnFront) 
	   {
		  pFront->Set(pvcFront, nNumFront, pnFront, (nNumFront-2)*3);

		  // make sure new one has same orientation as original
		  if (pFront->GetPlane().m_vcN * m_Plane.m_vcN < 0.0f) pFront->SwapFaces();
	   }

	   if (pBack && pnBack) 
	   {
		  pBack->Set(pvcBack,   nNumBack,  pnBack,  (nNumBack-2)*3);

		  // make sure new one has same orientation as original
		  if (pBack->GetPlane().m_vcN * m_Plane.m_vcN < 0.0f) pBack->SwapFaces();
	   }

	   // free temporary memory
	   if (pvcFront) { delete [] pvcFront; }
	   if (pvcBack)  { delete [] pvcBack; }
	   if (pnFront)  { delete [] pnFront; }
	   if (pnBack)   { delete [] pnBack; }
	   } // Clip
 

	// clips the poly that it fits into the given aabb
	void Polygon::Clip(const Aabb &aabb) 
	{
	   Polygon BackPoly, ClippedPoly;
	   Plane   Planes[6];
	   bool       bClipped=false;

	   // cast away const
	   Aabb *pAabb = ((Aabb*)&aabb);

	   // get the planes of the aabb, note that
	   // the normals are pointing outwards
	   pAabb->GetPlanes(Planes);

	   // make a copy of this polygon
	   ClippedPoly.CopyOf( *this );

	   // now clip the poly against the planes 
	   for (int i=0; i<6; i++) 
	   {
		  if ( Planes[i].Classify(ClippedPoly) == CLIPPED ) 
		  {
			 ClippedPoly.Clip(Planes[i], NULL, &BackPoly);
			 ClippedPoly.CopyOf(BackPoly);
			 bClipped = true;
		  }  
	   }

	   // set this poly to the final clip output
	   if (bClipped) 
	   {
		  CopyOf(ClippedPoly);
	   }
	} // Clip
 
	// checks if the polygon is inside, outisde or clipped by the aabb
	// CLIPPED CASE ONLY APPROXIMATED
	int Polygon::Cull(const Aabb &aabb) 
	{
	   Plane Planes[6];
	   int      nClass=0;
	   int      nInside=0, nCurrent=0;
	   bool     bFirst=true;
	   Ray   Ray;

	   // cast away const
	   Aabb *pAabb = ((Aabb*)&aabb);

	   // get the planes of the aabb, note that
	   // the normals are pointing outwards
	   pAabb->GetPlanes(Planes);

	   // check if polys aabb is roughly intersecting at all
	   if ( !m_Aabb.Intersects(aabb) )
		  return CULLED; // no way

	   // now polygon is eventually but not
	   // necessarily intersecting the aabb
	   for (int p=0; p<6; p++) 
	   {

		  // one time check if all points are inside the aabb
		  if (bFirst) 
		  {
			 for (int i=0; i<m_NumP; i++) 
			 {
				if ( pAabb->Intersects(m_pPoints[i]) )
				   nInside++;
			 }
			 bFirst = false;

			 // all points inside => poly totally inside aabb
			 if (nInside  == m_NumP) return VISIBLE;
		  }

		  // check for intersection of poly with aabb plane
		  for (int nLoop=1; nLoop < (m_NumP+1); nLoop++) 
		  {
			 if (nLoop == m_NumP) nCurrent = 0;
			 else nCurrent = nLoop;

			 // build ray from two neighboring points
			 Ray.m_vcOrig = m_pPoints[nLoop-1];
			 Ray.m_vcDir  = m_pPoints[nCurrent] - m_pPoints[nLoop-1];

			 float fLength = Ray.m_vcDir.GetLength();
			 if (fLength != 0.0f) Ray.m_vcDir /= fLength;

			 /* NEED TO CHECK WHETHER INTERSECTION POINT IS INSIDE AABB AT ALL */
			 if (Ray.Intersects(Planes[p], false, fLength, 0, NULL)) 
			   return CLIPPED;
			 }
		  }

	   // no intersection
	   return CULLED;
	} // Cull
 
	// makes this polygon a copy of the given polygon
	void Polygon::CopyOf(const Polygon &Poly) 
	{

	   Set(Poly.m_pPoints, Poly.m_NumP,Poly.m_pIndis,  Poly.m_NumI);

	} // CopyOf
 
	// intersection with a ray
	bool Polygon::Intersects(const Ray &ray, bool bCull, float *t) 
	{
	   // cast away const
	   Ray *pRay = (Ray*)&ray;

	   for (int i=0; i<m_NumI; i+=3) 
	   {
		  if (pRay->Intersects(m_pPoints[m_pIndis[i]],m_pPoints[m_pIndis[i+1]],m_pPoints[m_pIndis[i+2]],false, t))
		  {
			 return true;
		  }
		  if (!bCull) 
		  {
			 if (pRay->Intersects(m_pPoints[m_pIndis[i+2]],m_pPoints[m_pIndis[i+1]],m_pPoints[m_pIndis[i]],false, t)) 
			 {
				return true;
			 }
		   }
	   }
	   return false;
	} // Intersects
 
	// intersection with a line segment
	bool Polygon::Intersects(const Ray &ray, bool bCull, float fL, float *t) 
	{
	   // cast away const
	   Ray *pRay = (Ray*)&ray;

	   for (int i=0; i<m_NumI; i+=3) 
	   {
		  if (pRay->Intersects(m_pPoints[m_pIndis[i]],m_pPoints[m_pIndis[i+1]],m_pPoints[m_pIndis[i+2]],false, fL, t))
		  {
				return true;
		  }
		  if (!bCull) 
		  {
			 if (pRay->Intersects(m_pPoints[m_pIndis[i+2]],m_pPoints[m_pIndis[i+1]],m_pPoints[m_pIndis[i]],false, fL, t))
			 {
				return true;
			 }
		  }
		}
	   return false;
	} // Intersects
 

	// output polygon data to textfile
	void Polygon::Print(FILE *p) 
	{
	   if (!p) return;

	   fprintf(p, "POLYGON { \n");
	   fprintf(p, "   NumPoints %d \n", m_NumP);
	   fprintf(p, "   NumIndis  %d \n", m_NumI);
	   fprintf(p, "   Vertices { \n");

	   for (int i=0; i<m_NumP; i++) 
	   {
		  fprintf(p, "      (%f, %f, %f)\n", 
			 m_pPoints[i].x, m_pPoints[i].y, m_pPoints[i].z);
	   }
	   fprintf(p, "      } \n");
	   fprintf(p, "   } // END_POLYGON \n");
	} // Print
}