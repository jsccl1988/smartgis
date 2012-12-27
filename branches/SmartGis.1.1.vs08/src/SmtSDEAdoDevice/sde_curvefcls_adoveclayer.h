/*
File:    sde_curvefcls_adolayer.h

Desc:    SmtCurveFclsAdoLayer,ÇúÏßÒªËØÀàÍ¼²ã

Version: Version 1.0

Writter:  ³Â´ºÁÁ

Date:    2010.11.17

Copyright (c) 2010 CCL. All rights reserved.
*/
#ifndef _SDE_CURVEFCLS_ADOLAYER_H
#define _SDE_CURVEFCLS_ADOLAYER_H

#include "sde_ado.h"

namespace Smt_SDEAdo
{
	class SmtCurveFclsAdoLayer:public SmtAdoVecLayer
	{
	public:
		SmtCurveFclsAdoLayer(SmtDataSource *pOwnerDs);
		virtual ~SmtCurveFclsAdoLayer(void);

	public:
		bool                     Create(void);
	
		//feature
		long                     AppendFeature(const SmtFeature *pSmtFeature,bool bclone = false) ;
		long					 AppendFeatureBatch(const SmtFeature *pSmtFeature,bool bClone = false);

		long                     UpdateFeature(const SmtFeature *pSmtFeature);
		
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
		long                     AppendLineString(const SmtLineString *pLineString);
		long                     AppendSpline(const SmtSpline *pSpline);
		long                     AppendLinearRing(const SmtLinearRing *pLinearRing);
		long                     AppendArc(const SmtArc *pArc);

		long                     GetLineString(SmtGeometry *&pLineString);
		long                     GetSpline(SmtGeometry *&pSpline);
		long                     GetLinearRing(SmtGeometry *&pLinearRing);
		long                     GetArc(SmtGeometry *&pArc);
	};
}

#endif //_SDE_CURVEFCLS_ADOLAYER_H