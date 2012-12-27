#include "sde_mem.h"
#include "smt_api.h"

namespace Smt_SDEMem
{
   SmtMemDataSource::SmtMemDataSource(void)
   {

   }

   SmtMemDataSource::~SmtMemDataSource(void)
   {

   }

   bool SmtMemDataSource::Create(void)
   {
	   //////////////////////////////////////////////////////////////////////////
	   //...

	   return true;
   }
	
   bool SmtMemDataSource::Open()
   {
	   m_bOpen = true;
	   return true;
   }

   SmtDataSource * SmtMemDataSource::Clone(void) const 
   {
         SmtMemDataSource *pDs = new SmtMemDataSource();
		 pDs->SetInfo(m_dsInfo);

		 return (SmtDataSource*)pDs;
   }

   bool SmtMemDataSource::Close()
   {
       return true;
   }

   SmtVectorLayer	*SmtMemDataSource::CreateVectorLayer(const char *pszName,fRect &lyrRect,SmtFeatureType ftType)
   {
	   SmtMemVecLayer *pLayer = new SmtMemVecLayer(this);
	   pLayer->SetLayerFeatureType(ftType);
	   pLayer->SetLayerName(pszName);
	   pLayer->SetLayerRect(lyrRect);
	   if (!pLayer->Create())
		   SMT_SAFE_DELETE(pLayer);	

		SmtLayerInfo info;
		sprintf(info.szArchiveName,"%s%s",m_dsInfo.szUrl,pszName);
		strcpy(info.szName,pszName);
		info.unFeatureType = ftType;

		m_vLayerInfos.push_back(info);

       return (SmtVectorLayer*)pLayer;
   }

   SmtVectorLayer	*SmtMemDataSource::OpenVectorLayer(const char *pszLayerFileName)
   {
	   SmtMemVecLayer *pLayer = new SmtMemVecLayer(this);
	   if(pLayer->Open(pszLayerFileName))
	   {
		   pLayer->SetLayerName(pszLayerFileName);
		   pLayer->Fetch();
		   return (SmtVectorLayer*)pLayer;
	   }

       SMT_SAFE_DELETE(pLayer);

	   return NULL;
   }

   bool  SmtMemDataSource::DeleteVectorLayer(const char *pszName)
   {
	   return true;
   }

   //////////////////////////////////////////////////////////////////////////
   SmtRasterLayer* SmtMemDataSource::CreateRasterLayer(const char *szName,fRect &lyrRect,long lImageCode)
   {
	   SmtMemRasLayer *pLayer = new SmtMemRasLayer(this);
	   pLayer->SetLayerName(szName);
	   pLayer->SetLayerRect(lyrRect);
	   if (!pLayer->Create())
		   SMT_SAFE_DELETE(pLayer);	

	   SmtLayerInfo info;
	   sprintf(info.szArchiveName,"%s%s",m_dsInfo.szUrl,szName);
	   strcpy(info.szName,szName);
	   info.unFeatureType = SmtLayer_Ras;

	   m_vLayerInfos.push_back(info);

	   return (SmtRasterLayer*)pLayer;
   }

   SmtRasterLayer* SmtMemDataSource::OpenRasterLayer(const char *szName)
   {
	   SmtMemRasLayer *pLayer = new SmtMemRasLayer(this);
	   if(pLayer->Open(szName))
	   {
		   pLayer->SetLayerName(szName);
		   pLayer->Fetch();
		   return (SmtRasterLayer*)pLayer;
	   }

	   SMT_SAFE_DELETE(pLayer);

	   return NULL;
   }	

   bool SmtMemDataSource::DeleteRasterLayer(const char *szName)
   {
		return true;
   }

   //////////////////////////////////////////////////////////////////////////
   SmtTileLayer* SmtMemDataSource::CreateTileLayer(const char *szName,fRect &lyrRect,long lImageCode)
   {
	   SmtMemTileLayer *pLayer = new SmtMemTileLayer(this);
	   pLayer->SetLayerName(szName);
	   pLayer->SetLayerRect(lyrRect);
	   if (!pLayer->Create())
		   SMT_SAFE_DELETE(pLayer);	

	   SmtLayerInfo info;
	   sprintf(info.szArchiveName,"%s%s",m_dsInfo.szUrl,szName);
	   strcpy(info.szName,szName);
	   info.unFeatureType = SmtLayer_Tile;

	   m_vLayerInfos.push_back(info);

	   return (SmtTileLayer*)pLayer;
   }

   SmtTileLayer* SmtMemDataSource::OpenTileLayer(const char *szName)
   {
	   SmtMemTileLayer *pLayer = new SmtMemTileLayer(this);
	   if(pLayer->Open(szName))
	   {
		   pLayer->SetLayerName(szName);
		   pLayer->Fetch();
		   return (SmtTileLayer*)pLayer;
	   }

	   return NULL;
   }

   bool SmtMemDataSource::DeleteTileLayer(const char *szName)
   {
		return true;
   }
}