/*
File:    gis_3dfeature.h

Desc:    Smt3DFeature,要素

Version: Version 1.0

Writter:  陈春亮

Date:    2011.8.1

Copyright (c) 2010 CCL. All rights reserved.
*/
#ifndef _GIS_3D_FEATURE_H
#define _GIS_3D_FEATURE_H

#include "smt_core.h"
#include "smt_bas_struct.h"
#include "geo_3dgeometry.h"
#include "gis_attribute.h"
#include "rd3d_base.h"

using namespace Smt_Core;
using namespace Smt_3DGeo;
using namespace Smt_3Drd;

namespace Smt_GIS
{
	enum Smt3DFeatureType
	{
		//点状要素
		SmtFt3DDot,
		SmtFt3DAnno,
		//线状要素
		SmtFt3DCurve,
		//面状要素
		SmtFt3DSurface,
		//网状要素
		SmtFt3DMesh,
		//未知要素
		SmtFt3DUnknown
	};

	class SMT_EXPORT_CLASS Smt3DFeature
	{
	public:
		Smt3DFeature(void);
		virtual ~Smt3DFeature(void);
		
		inline long                GetID(void) {return m_lID;}
		inline Smt3DFeatureType    GetFeatureType(void) {return m_SmtFeatureType;}
		inline Smt3DGeometry *	   GetGeometryRef(void) {return m_pGeom;}
		inline SmtAttribute *      GetAttributeRef(void) {return m_pAtt;}

		//////////////////////////////////////////////////////////////////////////
		inline void                SetID(long id) {m_lID = id;}
		inline void                SetFeatureType(Smt3DFeatureType type);
		void                       SetGeometryDirectly(Smt3DGeometry  *pGeom);
		void                       SetGeometry(Smt3DGeometry  *pGeom);

		Smt3DFeature *             Clone();

		virtual bool               Equal( Smt3DFeature * pFeature );

	    //////////////////////////////////////////////////////////////////////////
		void                       AddField(SmtField & fld);
		void                       RemoveField(const char * szFldName);
		void                       SetName(int index,const char * szName);
		void                       SetType(int index,varType uVt);

		int                        GetFieldIndexByName(const char * szName);

		int                        SetFieldValue(int index,const SmtVariant &smtFld);
		int                        SetFieldValue(int index, int nValue );
		int                        SetFieldValue(int index, double dfValue );
		int                        SetFieldValue(int index, byte bValue );
		int                        SetFieldValue(int index, bool bValue );
		int                        SetFieldValue(int index, const char * pszValue );
		int                        SetFieldValue(int index, int nCount, int * panValues );
		int                        SetFieldValue(int index, int nCount, double * padfValues );
		int                        SetFieldValue(int index, char ** papszValues );
		int                        SetFieldValue(int index, int nCount, byte * pabyBinary );
		int                        SetFieldValue(int index, int nYear, int nMonth, int nDay,int nHour=0, int nMinute=0, int nSecond=0, int nTZFlag = 0 );

		////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
		//material
		int                        SetMaterial(const char * matname);
		int                        SetMaterialDirectly(SmtMaterial * pMaterial);
		int                        SetMaterial(SmtMaterial * pMaterial);
		SmtMaterial*			   GetMaterial(void) {return m_pMaterial;}

	protected:
		long                        m_lID;
		Smt3DFeatureType            m_SmtFeatureType;
		SmtAttribute                *m_pAtt;
		Smt3DGeometry				*m_pGeom; 
		SmtMaterial					*m_pMaterial;
	};
}

#if !defined(Export_SmtGisCore)
#if   defined( _DEBUG)
#          pragma comment(lib,"SmtGisCoreD.lib")
#       else
#          pragma comment(lib,"SmtGisCore.lib")
#	    endif  
#endif

#endif //_GIS_3D_FEATURE_H