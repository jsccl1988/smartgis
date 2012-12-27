/*
File:    sde_dotfcls_adolayer.h

Desc:    SmtDotFclsAdoLayer,µ„“™Àÿ¿‡Õº≤„

Version: Version 1.0

Writter:  ≥¬¥∫¡¡

Date:    2010.11.17

Copyright (c) 2010 CCL. All rights reserved.
*/
#ifndef _SDE_DOTFCLS_ADOLAYER_H
#define _SDE_DOTFCLS_ADOLAYER_H

#include "sde_ado.h"

namespace Smt_SDEAdo
{
	class SmtDotFclsAdoLayer:public SmtAdoVecLayer
	{
	public:
		SmtDotFclsAdoLayer(SmtDataSource *pOwnerDs);
		virtual ~SmtDotFclsAdoLayer(void);

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

		long                     AppendGeom(const SmtGeometry *pGeom);
		long                     GetGeom(SmtGeometry *&pGeom);

		long					 GetLayerAttribute(void);

	private:
		long                     AppendPoint(const SmtPoint *pPoint);
		long                     AppendPoints(const SmtMultiPoint *pPoints);
		long                     GetPoint(SmtGeometry *&pPoint);
		long                     GetPoints(SmtGeometry *&pPoints);
	};
}

#endif //_SDE_DOTFCLS_ADOLAYER_H