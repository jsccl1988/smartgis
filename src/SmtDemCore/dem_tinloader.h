/*
File:   dem_tinloader.h

Desc:    Smt3DTinLoader,Èý½ÇÍø

Version: Version 1.0

Writter:  ³Â´ºÁÁ

Date:    2012.2.17

Copyright (c) 2010 CCL. All rights reserved.
*/
#ifndef _DEM_TIN_H
#define _DEM_TIN_H

#include "smt_core.h"
#include "geo_3dgeometry.h"

using namespace Smt_Core;
using namespace Smt_3DGeo;

namespace Smt_DEM
{
	enum SeparatorType
	{
		ST_TAB,
		ST_SPACE,
		ST_COMMA
	};

	struct SmtTinFileFmt 
	{	
		int		nSeparatorType;			//0 TAB,1 ¿Õ¸ñ,1 ","
		int		nCol;
		int		iX,iY,iZ;
		int		nHeadSkip;
		int		nLineSkip;

		SmtTinFileFmt(void):nCol(0)
			,iX(0)
			,iY(0)
			,iZ(0)
			,nSeparatorType(0)
			,nHeadSkip(0)
			,nLineSkip(0)
		{
			;
		}

		SmtTinFileFmt(int _nCol,int _iX,int _iY,int _iZ,int _nSeparatorType,int _nHeadSkip,int _nLineSkip):nCol(_nCol)
			,iX(_iX)
			,iY(_iY)
			,iZ(_iZ)
			,nSeparatorType(_nSeparatorType)
			,nHeadSkip(_nHeadSkip)
			,nLineSkip(_nLineSkip)
		{
			;
		}
	};
	class SMT_EXPORT_CLASS Smt3DTinLoader
	{
	public:
		Smt3DTinLoader(void);
		virtual ~Smt3DTinLoader(void);

	public:
		long					LoadDataFromASSII(const char * szFileName,SmtTinFileFmt &fileFmt);

	public:
		//////////////////////////////////////////////////////////////////////////
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


		inline Smt3DSurface		*GetTinSurf() {return &m_tinSurf;}
	
	protected:
		float					m_fMinX;
		float					m_fMaxX;

		float					m_fMinY;
		float					m_fMaxY;

		float					m_fMinZ;
		float					m_fMaxZ;

		float					m_fZScale;
		float					m_fXScale;
		float					m_fYScale;

		Smt3DSurface			m_tinSurf;
	};
}

#if !defined(Export_SmtDemCore)
#if   defined( _DEBUG)
#          pragma comment(lib,"SmtDemCoreD.lib")
#       else
#          pragma comment(lib,"SmtDemCore.lib")
#	    endif  
#endif

#endif //_DEM_GRID_H