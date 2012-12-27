/*
File:   dem_gridloader.h

Desc:    Smt3DGridLoader,¸ñÍø

Version: Version 1.0

Writter:  ³Â´ºÁÁ

Date:    2012.2.17

Copyright (c) 2010 CCL. All rights reserved.
*/
#ifndef _DEM_GRIDLOADER_H
#define _DEM_GRIDLOADER_H

#include "smt_core.h"
#include "geo_3dgeometry.h"

using namespace Smt_Core;
using namespace Smt_3DGeo;

namespace Smt_DEM
{
	class SMT_EXPORT_CLASS Smt3DGridLoader
	{
	public:
		Smt3DGridLoader(void);
		virtual ~Smt3DGridLoader(void);

	public:
		long					LoadDataFromHeightMap(const char * szFileName);

	public:
		//////////////////////////////////////////////////////////////////////////
		inline void				SetXStart(float fXStart) {m_fXStart = fXStart;}
		inline void				SetYStart(float fYStart) {m_fYStart = fYStart;}
		inline void				SetZStart(float fYStart) {m_fZStart = fYStart;}

		inline float			GetXStart(void){return m_fXStart;}
		inline float			GetYStart(void){return m_fYStart;}
		inline float			GetZStart(void){return m_fZStart;}

		inline void				SetXScale(float fScale){m_fXScale = fScale;}
		inline void				SetYScale(float fScale){m_fYScale = fScale;}
		inline void				SetZScale(float fScale){m_fZScale = fScale;}

		inline float			GetXScale(void){return m_fXScale;}
		inline float			GetYScale(void){return m_fYScale;}
		inline float			GetZScale(void){return m_fZScale;}

		inline float			GetMaxX(){return m_fMaxX;}
		inline float			GetMinX(){return m_fMinX;}

		inline float			GetMaxY(){return m_fMaxY;}
		inline float			GetMinY(){return m_fMinY;}

		inline float			GetMaxZ(){return m_fMaxZ;}
		inline float			GetMinZ(){return m_fMinZ;}

		
		inline Smt3DSurface		*GetGridSurf() {return &m_gridSurf;}

	protected:
		inline char				GetTrueZAtIndex(int iX ,int  iY ){return (m_pData[(iY*m_nX)+iX]);}
		inline float			GetScaledZAtIndex(int iX ,int iY){return (m_pData[(iY*m_nX)+iX] * m_fZScale+m_fZStart);}	
		
		long					CreateGridSurf(void);

	protected:
		char unsigned *			m_pData;
		int						m_nX;	         // X
		int						m_nY;	         // Y

		float					m_fXStart;
		float					m_fYStart;
		float					m_fZStart;

		float					m_fMinX;
		float					m_fMaxX;

		float					m_fMinY;
		float					m_fMaxY;

		float					m_fMinZ;
		float					m_fMaxZ;

		float					m_fZScale;
		float					m_fXScale;
		float					m_fYScale;

		Smt3DSurface			m_gridSurf;
		
	};
}

#if !defined(Export_SmtDemCore)
#if   defined( _DEBUG)
#          pragma comment(lib,"SmtDemCoreD.lib")
#       else
#          pragma comment(lib,"SmtDemCore.lib")
#	    endif  
#endif

#endif //_DEM_GRIDLOADER_H