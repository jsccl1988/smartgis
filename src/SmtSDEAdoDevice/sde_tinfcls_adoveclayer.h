#ifndef _SDE_TINFCLS_ADOLAYER_H
#define _SDE_TINFCLS_ADOLAYER_H

#include "sde_ado.h"

namespace Smt_SDEAdo
{
	class SmtTinFclsAdoLayer:public SmtAdoVecLayer
	{
	public:
		SmtTinFclsAdoLayer(SmtDataSource *pOwnerDs);
		virtual ~SmtTinFclsAdoLayer(void);

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
		long                     AppendTin(const SmtTin *pTin);
		long                     AppendTinTriangles(const SmtTin *pTin);
		long                     AppendTinNodes(const SmtTin *pTin);
		
		long                     GetTin(SmtGeometry *&pTin);
		long                     GetTinTriangles(SmtTin *&pTin);
		long                     GetTinNodes(SmtTin *&pTin);
	};
}

#endif //_SDE_TINFCLS_ADOLAYER_H