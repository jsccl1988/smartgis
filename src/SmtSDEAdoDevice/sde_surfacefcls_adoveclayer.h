/*
File:    sde_surfacefcls_adolayer.h

Desc:    SmtSurfaceFclsAdoLayer,ÇøÒªËØÀàÍ¼²ã

Version: Version 1.0

Writter:  ³Â´ºÁÁ

Date:    2010.11.17

Copyright (c) 2010 CCL. All rights reserved.
*/
#ifndef _SDE_SURFACEFCLS_ADOLAYER_H
#define _SDE_SURFACEFCLS_ADOLAYER_H

#include "sde_ado.h"

namespace Smt_SDEAdo
{
	class SmtSurfaceFclsAdoLayer:public SmtAdoVecLayer
	{
	public:
		SmtSurfaceFclsAdoLayer(SmtDataSource *pOwnerDs);
		virtual ~SmtSurfaceFclsAdoLayer(void);

	public:
		bool                     Create(void);
	
		//feature
		long                     AppendFeature(const SmtFeature *pSmtFeature,bool bclone = false) ;
		long					 AppendFeatureBatch(const SmtFeature *pSmtFeature,bool bClone = false);

		long                     UpdateFeature(const SmtFeature *pSmtFeature);
	   //////////////////////////////////////////////////////////////////////////
	protected:
		void					 SetDefaultAttFields(void);
		void					 SetDefaultGeomFields(void);
		void					 SetTableFields(void);

		void                     GetFeature(SmtFeature *pSmtFeature);

	private:
		long                     AppendStyle(const SmtStyle *pStyle);
		long                     GetStyle(SmtStyle *&pStyle);  

	private:
		long                     AppendGeom(const SmtGeometry *pGeom);
		long                     GetGeom(SmtGeometry *&pGeom);

	private:
		long                     AppendPolygon(const SmtPolygon * pPolygon);
		long                     PutCollectLinearingNums(const SmtPolygon * pPolygon);
		long                     PutCollectLinearings(const SmtPolygon * pPolygon);
		long                     PutCollectLinearing(const SmtLinearRing * pLinearRing,SAFEARRAY *pSa,long &index);

		long                     AppendFan(const SmtFan * pFan);

		long                     GetPolygon(SmtGeometry * &pPolygon);
		long                     GetCollectLinearingNums(SmtPolygon *pPloygon);
		long                     GetCollectLinearings(SmtPolygon *pPloygon);
		long                     GetCollectLinearing(SmtLinearRing *pLinearRing,RawPoint *&pRawPointBuf);

		long                     GetFan(SmtGeometry *&pFan);
	};
}

#endif //_SDE_SURFACEFCLS_ADOLAYER_H