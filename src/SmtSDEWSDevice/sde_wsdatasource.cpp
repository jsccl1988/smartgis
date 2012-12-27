#include "sde_ws.h"
#include "smt_api.h"

namespace Smt_SDEWS
{
   SmtWSDataSource::SmtWSDataSource(void)
   {

   }

   SmtWSDataSource::~SmtWSDataSource(void)
   {

   }

   bool SmtWSDataSource::Create(void)
   {
	   //////////////////////////////////////////////////////////////////////////
	   //...

	   return true;
   }
	
   bool SmtWSDataSource::Open()
   {
	   m_bOpen = true;
	   return true;
   }

   SmtDataSource * SmtWSDataSource::Clone(void) const 
   {
         SmtWSDataSource *pDs = new SmtWSDataSource();
		 pDs->SetInfo(m_dsInfo);

		 return (SmtDataSource*)pDs;
   }

   bool SmtWSDataSource::Close()
   {
       return true;
   }

   SmtTileLayer* SmtWSDataSource::CreateTileLayer(const char *szName,fRect &lyrRect,long lImageCode)
   {
	   SmtWSTileLayer *pLayer = new SmtWSTileLayer(this);
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

   SmtTileLayer* SmtWSDataSource::OpenTileLayer(const char *szName)
   {
	   SmtWSTileLayer *pLayer = new SmtWSTileLayer(this);
	   if(pLayer->Open(szName))
	   {
		   pLayer->SetLayerName(szName);
		   pLayer->Fetch();

		   return (SmtTileLayer*)pLayer;
	   }

	   return NULL;
   }

   bool SmtWSDataSource::DeleteTileLayer(const char *szName)
   {
	   return true;
   }
}