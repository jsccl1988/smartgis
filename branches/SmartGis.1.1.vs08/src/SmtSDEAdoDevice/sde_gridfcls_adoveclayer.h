#ifndef _SDE_GRIDFCLS_ADOLAYER_H
#define _SDE_GRIDFCLS_ADOLAYER_H

#include "sde_ado.h"

namespace Smt_SDEAdo
{
	class SmtGridFclsAdoLayer:public SmtAdoVecLayer
	{
	public:
		SmtGridFclsAdoLayer(SmtDataSource *pOwnerDs);
		virtual ~SmtGridFclsAdoLayer(void);

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
		long                     AppendGrid(const SmtGrid *pGrid);
		long                     AppendGridSize(const SmtGrid *pGrid);
		long                     AppendGridNodes(const SmtGrid *pGrid);
		
		long                     GetGrid(SmtGeometry *&pGrid);
		long                     GetGridSize(SmtGrid *&pGrid);
		long                     GetGridNodes(SmtGrid *&pGrid);
	};
}

#endif //_SDE_GRIDFCLS_ADOLAYER_H