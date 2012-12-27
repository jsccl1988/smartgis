/*
File:    gis_map.h

Desc:    SmtMap,µØÍ¼ÎÄµµÀà

Version: Version 1.0

Writter:  ³Â´ºÁÁ

Date:    2010.11.17

Copyright (c) 2010 CCL. All rights reserved.
*/
#ifndef _GIS_MAP_H
#define _GIS_MAP_H

#include "gis_feature.h"
#include "gis_sde.h"

using namespace Smt_Base;
using namespace Smt_Core;

#define  MAX_MAP_NAME                 MAX_NAME_LENGTH

namespace Smt_GIS
{
	class SMT_EXPORT_CLASS SmtMap
	{
	public:
		SmtMap(void);
		virtual ~SmtMap(void);

	public:
		bool                        AddLayer(const SmtLayer * pLayer);

		bool                        DeleteLayer(const char * szName);
		bool                        DeleteLayer(const SmtLayer *pLayer);

		bool                        MoveTo (int fromIndex, int toIndex);	
		bool                        MoveToBottom (int index);		
		bool                        MoveToTop (int index);

		void                        SetActiveLayer(const char * szName) {m_pCurrentLayer = GetLayer(szName);}

		SmtLayer*					GetActiveLayer(void) {return m_pCurrentLayer;} 
		const SmtLayer*				GetActiveLayer(void) const {return m_pCurrentLayer;} 

		SmtLayer*					GetLayer(const char * szName); 
		const SmtLayer*				GetLayer(const char * szName) const; 

		int                         GetLayerCount(void) {return m_vSmtLayers.size();}

		virtual bool				AppendFeature(const SmtFeature *pFeature,bool bIsClone = false);
		virtual bool				DeleteFeature(const SmtFeature *pFeature);
		virtual bool				UpdateFeature(const SmtFeature *pFeature);

		virtual bool				QueryFeature(const SmtGQueryDesc *  pGQueryDesc,const SmtPQueryDesc * pPQueryDesc,SmtVectorLayer * pQueryResult,int &nFeaType);

		//////////////////////////////////////////////////////////////////////////
		void                        MoveFirst(void) const ;
		void                        MoveNext(void) const ;
		void                        MoveLast(void) const ;
		void                        Delete(void);
		bool                        IsEnd(void) const ;

		void                        DeleteAll(void);

		SmtLayer *					GetLayer();
		const SmtLayer *			GetLayer() const ;

		SmtLayer *					GetLayer(int index);
		const SmtLayer *			GetLayer(int index) const ;

		//////////////////////////////////////////////////////////////////////////
		void                        SetMapName(const char *szName) { strcpy_s(m_szMapName,MAX_MAP_NAME,szName);}
		const char *                GetMapName(void) const{return m_szMapName;}

		//////////////////////////////////////////////////////////////////////////
		void                        GetEnvelope(Envelope &env) const { memcpy(&env,&m_MapEnvelope,sizeof(Envelope));}
		void                        CalEnvelope(void);

	protected:
		char                        m_szMapName[MAX_MAP_NAME];
		Envelope					m_MapEnvelope;
		vector<SmtLayer*>			m_vSmtLayers;
		SmtLayer*					m_pCurrentLayer;
		mutable int                 m_nIteratorIndex;
	};
}

#if !defined(Export_SmtGisCore)
#if   defined( _DEBUG)
#          pragma comment(lib,"SmtGisCoreD.lib")
#       else
#          pragma comment(lib,"SmtGisCore.lib")
#	    endif  
#endif

#endif //_GIS_LAYER_H