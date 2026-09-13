/*
File:    gis_3dfeature.h

Desc:    Smt3DFeature,Ҫ��

Version: Version 1.0

Writter:  �´���

Date:    2011.8.1

Copyright (c) 2010 CCL. All rights reserved.
*/
#ifndef _GIS_3D_FEATURE_H
#define _GIS_3D_FEATURE_H

#include "base/core/core.h"
#include "base/core/bas_struct.h"
#include "algorithm/geo/geometry.h"
#include "sdb/feature/attribute.h"
#include "sdb/gis_export.h"
#include "render/render3d/base.h"

using namespace base;
using namespace geo;
using namespace render;

namespace sdb
{
	enum Smt3DFeatureType
	{
		//��״Ҫ��
		SmtFt3DDot,
		SmtFt3DAnno,
		//��״Ҫ��
		SmtFt3DCurve,
		//��״Ҫ��
		SmtFt3DSurface,
		//��״Ҫ��
		SmtFt3DMesh,
		//δ֪Ҫ��
		SmtFt3DUnknown
	};

	class GIS_EXPORT Smt3DFeature
	{
	public:
		Smt3DFeature(void);
		virtual ~Smt3DFeature(void);
		
		inline long                GetID(void) {return m_lID;}
		inline Smt3DFeatureType    GetFeatureType(void) {return m_SmtFeatureType;}
		inline OGRGeometry *	   GetGeometryRef(void) {return m_pGeom;}
		inline SmtAttribute *      GetAttributeRef(void) {return m_pAtt;}

		//////////////////////////////////////////////////////////////////////////
		inline void                SetID(long id) {m_lID = id;}
		inline void                SetFeatureType(Smt3DFeatureType type);
		void                       SetGeometryDirectly(OGRGeometry  *pGeom);
		void                       SetGeometry(OGRGeometry  *pGeom);

		Smt3DFeature *             clone();

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
		OGRGeometry				*m_pGeom; 
		SmtMaterial					*m_pMaterial;
	};
}

#endif  // _GIS_3D_FEATURE_H