#include "sde_smf.h"
#include "smt_api.h"
#include "ogrsf_frmts.h"
#include "sde_smf_api.h"
#include "smt_logmanager.h"

namespace Smt_SDESmf
{
   SmtSmfDataSource::SmtSmfDataSource(void)
   {
	    SmtLogManager * pLogMgr = SmtLogManager::GetSingletonPtr();
		SmtLog *pLog = pLogMgr->GetLog(C_STR_SDE_SMFDEVICE_LOG);
		if (NULL == pLog)
		{
			pLogMgr->CreateLog(C_STR_SDE_SMFDEVICE_LOG.c_str());
		}
   }

   SmtSmfDataSource::~SmtSmfDataSource(void)
   {

   }

   bool SmtSmfDataSource::Open(void)
   {
	   m_bOpen = true;

	   SmtLogManager * pLogMgr = SmtLogManager::GetSingletonPtr();
	   SmtLog *pLog = pLogMgr->GetLog(C_STR_SDE_SMFDEVICE_LOG);

	   pLog->LogMessage("DS-Name:%s,FILE Name:%s,DB Service:%s,User:%s,type:%d,provider:%d",m_dsInfo.szName,m_dsInfo.file.szFileName,m_dsInfo.file.szPath,m_dsInfo.szUID,m_dsInfo.unType,m_dsInfo.unProvider);

	   string strUrl;
	   strUrl += m_dsInfo.file.szPath;
	   strUrl += m_dsInfo.file.szFileName;
	   strUrl += ".smf";

	   m_vLayerInfos.clear();
	   ReadSmf(strUrl.c_str(),m_vLayerInfos);

	   OGRRegisterAll();

	   return true;
   }

   bool SmtSmfDataSource::Close()
   {
	   return true;
   }

   bool SmtSmfDataSource::Create(void)
   {
	   return true;
   }

   SmtDataSource *SmtSmfDataSource::Clone(void) const
   {
         SmtSmfDataSource *pDs = new SmtSmfDataSource();
		 pDs->SetInfo(m_dsInfo);

		 return (SmtDataSource*)pDs;
   }
 
   SmtVectorLayer	*SmtSmfDataSource::CreateVectorLayer(const char *szName,fRect &lyrRect,SmtFeatureType ftType)
   {
	   SmtSmfVecLayer *pLayer = new SmtSmfVecLayer(this);

	   pLayer->SetLayerFeatureType(ftType);
	   pLayer->SetLayerName(szName);
	   pLayer->SetLayerRect(lyrRect);

	   if (!pLayer->Create())
		   SMT_SAFE_DELETE(pLayer);	

		SmtLayerInfo info;
		sprintf(info.szArchiveName,"%s%s",m_dsInfo.file.szPath,szName);
		strcpy(info.szName,szName);
		info.unFeatureType = ftType;

		m_vLayerInfos.push_back(info);

		string strUrl;
		strUrl += m_dsInfo.file.szPath;
		strUrl += m_dsInfo.file.szFileName;
		strUrl += ".smf";

		WriteSmf(strUrl.c_str(),m_vLayerInfos);

       return (SmtVectorLayer*)pLayer;
   }

   SmtVectorLayer	*SmtSmfDataSource::OpenVectorLayer(const char *szLayerName)
   {
	   if (m_vLayerInfos.size() == 0)
	   {
		   SmtSmfVecLayer *pLayer = new SmtSmfVecLayer(this);
		   if(pLayer->Open(szLayerName))
		   {
			   pLayer->SetLayerName(szLayerName);
			 //  pLayer->SetLayerFeatureType(SmtFeatureType((*iter).unFeatureType));
			   if (pLayer->Fetch())
				   return (SmtVectorLayer*)pLayer;
		   }

		   SMT_SAFE_DELETE(pLayer);

		   return NULL;
	   }

	   vector<SmtLayerInfo>::iterator iter = m_vLayerInfos.begin() ;

	   while (iter != m_vLayerInfos.end())
	   {
		   if (strcmp((*iter).szName,szLayerName) == 0)
		   {
			   SmtSmfVecLayer *pLayer = new SmtSmfVecLayer(this);
			   if(pLayer->Open((*iter).szArchiveName))
			   {
				   pLayer->SetLayerName((*iter).szName);
				   pLayer->SetLayerFeatureType(SmtFeatureType((*iter).unFeatureType));
				   if (pLayer->Fetch())
					   return (SmtVectorLayer*)pLayer;
			   }

			   SMT_SAFE_DELETE(pLayer);

			   return NULL;
		   }

		   ++iter;
	   }

	   return NULL;
   }

   bool  SmtSmfDataSource::DeleteVectorLayer(const char *szName)
   {
	   vector<SmtLayerInfo>::iterator iter = m_vLayerInfos.begin() ;

	   while (iter != m_vLayerInfos.end())
	   {
		   if (strcmp((*iter).szName,szName) == 0)
		   {
			   m_vLayerInfos.erase(iter);
			   string strUrl;
			   strUrl += m_dsInfo.file.szPath;
			   strUrl += m_dsInfo.file.szFileName;
			   strUrl += ".smf";
			   WriteSmf(strUrl.c_str(),m_vLayerInfos);

			   return true;
		   }
		   ++iter;
	   }

	  return false;
   }

   SmtRasterLayer* SmtSmfDataSource::CreateRasterLayer(const char *szName,fRect &lyrRect,long lImageCode)
   {
	   SmtSmfRasLayer *pLayer = new SmtSmfRasLayer(this);

	   pLayer->SetLayerName(szName);
	   pLayer->SetLayerRect(lyrRect);

	   if (!pLayer->Create())
		   SMT_SAFE_DELETE(pLayer);	

	   SmtLayerInfo info;
	   sprintf(info.szArchiveName,"%s%s",m_dsInfo.file.szPath,szName);
	   strcpy(info.szName,szName);
	   info.unFeatureType = SmtLayer_Ras;
	   info.lyrRect = lyrRect;

	   m_vLayerInfos.push_back(info);

	   string strUrl;
	   strUrl += m_dsInfo.file.szPath;
	   strUrl += m_dsInfo.file.szFileName;
	   strUrl += ".smf";

	   WriteSmf(strUrl.c_str(),m_vLayerInfos);

	   return pLayer;
   }

   SmtRasterLayer* SmtSmfDataSource::OpenRasterLayer(const char *szLayerName)
   {
	   if (m_vLayerInfos.size() == 0)
	   {
		   SmtSmfRasLayer *pLayer = new SmtSmfRasLayer(this);
		   if(pLayer->Open(szLayerName))
		   {
			   pLayer->SetLayerName(szLayerName);
			   //pLayer->SetLayerRect((*iter).lyrRect);

			   if (pLayer->Fetch())
				   return (SmtSmfRasLayer*)pLayer;
		   }

		   SMT_SAFE_DELETE(pLayer);
	   }

	   vector<SmtLayerInfo>::iterator iter = m_vLayerInfos.begin() ;
	   while (iter != m_vLayerInfos.end())
	   {
		   if (strcmp((*iter).szName,szLayerName) == 0)
		   {
			   SmtSmfRasLayer *pLayer = new SmtSmfRasLayer(this);
			   if(pLayer->Open((*iter).szArchiveName))
			   {
				   pLayer->SetLayerName((*iter).szName);
				   pLayer->SetLayerRect((*iter).lyrRect);

				   if (pLayer->Fetch())
					   return (SmtSmfRasLayer*)pLayer;
			   }

			   SMT_SAFE_DELETE(pLayer);

			   return NULL;
		   }

		   ++iter;
	   }

	   return NULL;
   }	

   bool SmtSmfDataSource::DeleteRasterLayer(const char *szName)
   {
	   vector<SmtLayerInfo>::iterator iter = m_vLayerInfos.begin() ;

	   while (iter != m_vLayerInfos.end())
	   {
		   if (strcmp((*iter).szName,szName) == 0)
		   {
			   m_vLayerInfos.erase(iter);
			   string strUrl;
			   strUrl += m_dsInfo.file.szPath;
			   strUrl += m_dsInfo.file.szFileName;
			   strUrl += ".smf";
			   WriteSmf(strUrl.c_str(),m_vLayerInfos);

			   return true;
		   }
		   ++iter;
	   }

	   return false;
   }
}