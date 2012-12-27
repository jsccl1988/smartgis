#include "sde_ado.h"
#include "smt_api.h"

#include "sde_dotfcls_adoveclayer.h"
#include "sde_cldimgfcls_adoveclayer.h"
#include "sde_annofcls_adoveclayer.h"
#include "sde_curvefcls_adoveclayer.h"
#include "sde_surfacefcls_adoveclayer.h"
#include "sde_gridfcls_adoveclayer.h"
#include "sde_tinfcls_adoveclayer.h"
#include "smt_logmanager.h"

namespace Smt_SDEAdo
{
   SmtAdoDataSource::SmtAdoDataSource(void)
   {
	   SmtLogManager * pLogMgr = SmtLogManager::GetSingletonPtr();
	   SmtLog *pLog = pLogMgr->GetLog(C_STR_SDE_ADODEVICE_LOG);
	   if (NULL == pLog)
	   {
		   pLogMgr->CreateLog(C_STR_SDE_ADODEVICE_LOG.c_str());
	   }
   }

   SmtAdoDataSource::~SmtAdoDataSource(void)
   {
        Close();
   }

   bool SmtAdoDataSource::Create(void)
   {
	   return true;
   }
	
   bool SmtAdoDataSource::Open(void)
   {   
	    Close();

		SmtLogManager * pLogMgr = SmtLogManager::GetSingletonPtr();
		SmtLog *pLog = pLogMgr->GetLog(C_STR_SDE_ADODEVICE_LOG);

		pLog->LogMessage("DS-Name:%s,DB Name:%s,DB Service:%s,User:%s,type:%d,provider:%d",m_dsInfo.szName,m_dsInfo.db.szDBName,m_dsInfo.db.szService,m_dsInfo.szUID,m_dsInfo.unType,m_dsInfo.unProvider);
	
		switch (m_dsInfo.unProvider)
		{
		case eSmtDBProvider::PROVIDER_ACCESS:
			{
				string strAcessAllPath;
				strAcessAllPath += m_dsInfo.db.szService;
				strAcessAllPath += m_dsInfo.db.szDBName;
				strAcessAllPath += ".mdb";
				m_bOpen = m_SmtConnection.ConnectAccess(strAcessAllPath.c_str(),m_dsInfo.szUID,m_dsInfo.szPWD);
			}
			break;
		case eSmtDBProvider::PROVIDER_SQLSERVER:
			{
				m_bOpen = m_SmtConnection.ConnectSqlServer(m_dsInfo.db.szService,m_dsInfo.db.szDBName,m_dsInfo.szUID,m_dsInfo.szPWD);
			}
			break;
		}
		
	   if (m_bOpen)
	   {
		   m_SmtRecordset.SetAdoConnection(&m_SmtConnection);

		   if (IsTableExist(DS_TABLE_NAME))
			  GetLayerTableInfos();
		   else 
              CreateDsTable();
	   }

	   return m_bOpen;
   }

   SmtDataSource *SmtAdoDataSource::Clone(void) const
   {
	   SmtAdoDataSource *pDs = new SmtAdoDataSource();
	   pDs->SetInfo(m_dsInfo);

	   return (SmtDataSource*)pDs;
   }

   bool SmtAdoDataSource::Close()
   {
	   m_vLayerInfos.clear();
	   m_SmtConnection.Close();
	   m_SmtRecordset.Close();

       return true;
   }

   //////////////////////////////////////////////////////////////////////////

   SmtVectorLayer	*SmtAdoDataSource::CreateVectorLayer(const char *pszName,fRect &lyrRect,SmtFeatureType ftType)
   {
	   SmtAdoVecLayer *pLayer = NULL;

	   switch(ftType)
	   {
	   case SmtFtDot:
		   {
			   pLayer = new SmtDotFclsAdoLayer(this);
			   pLayer->SetLayerFeatureType(ftType);
			   pLayer->SetLayerName(pszName);
			   pLayer->SetLayerRect(lyrRect);

			   if (!pLayer->Create())
				   SMT_SAFE_DELETE(pLayer);		   
		   }
		   break;
	   case SmtFtChildImage:
		   {
			   pLayer = new SmtCldImgFclsDBLayer(this);
			   pLayer->SetLayerFeatureType(ftType);
			   pLayer->SetLayerName(pszName);
			   pLayer->SetLayerRect(lyrRect);

			   if (!pLayer->Create())
				   SMT_SAFE_DELETE(pLayer);		   
		   }
		   break;
	   case SmtFtAnno:
		   {
			   pLayer = new SmtAnnoFclsAdoLayer(this);
			   pLayer->SetLayerFeatureType(ftType);
			   pLayer->SetLayerName(pszName);
			   pLayer->SetLayerRect(lyrRect);

			   if (!pLayer->Create())
				   SMT_SAFE_DELETE(pLayer);		   
		   }
		   break;

	   case SmtFtCurve:
		   {
			   pLayer = new SmtCurveFclsAdoLayer(this);
			   pLayer->SetLayerFeatureType(ftType);
			   pLayer->SetLayerName(pszName);
			   pLayer->SetLayerRect(lyrRect);

			   if (!pLayer->Create())
				   SMT_SAFE_DELETE(pLayer);		   
		   }
		   break;

	   case SmtFtSurface:
		   {
			   pLayer = new SmtSurfaceFclsAdoLayer(this);
			   pLayer->SetLayerFeatureType(ftType);
			   pLayer->SetLayerName(pszName);
			   pLayer->SetLayerRect(lyrRect);

			   if (!pLayer->Create())
				   SMT_SAFE_DELETE(pLayer);		   
		   }
		   break;
	   case SmtFtGrid:
		   {
			   pLayer = new SmtGridFclsAdoLayer(this);
			   pLayer->SetLayerFeatureType(ftType);
			   pLayer->SetLayerName(pszName);
			   pLayer->SetLayerRect(lyrRect);

			   if (!pLayer->Create())
				   SMT_SAFE_DELETE(pLayer);		   
		   }
		   break;
	   case SmtFtTin:
		   {
			   pLayer = new SmtTinFclsAdoLayer(this);
			   pLayer->SetLayerFeatureType(ftType);
			   pLayer->SetLayerName(pszName);
			   pLayer->SetLayerRect(lyrRect);

			   if (!pLayer->Create())
				   SMT_SAFE_DELETE(pLayer);		   
		   }
		   break;
	   default:
		   break;
	   }

	   return (SmtVectorLayer*)pLayer;
   }

   SmtVectorLayer	*SmtAdoDataSource::OpenVectorLayer(const char *szName)
   {
	   SmtLayerInfo info;

	   if (!GetLayerTableInfoByLayerName(info,szName))
		   return NULL;

	   SmtAdoVecLayer *pLayer = NULL;

	   switch(info.unFeatureType)
	   {
	   case SmtFtDot:
		   {
			   pLayer = new SmtDotFclsAdoLayer(this);
			   pLayer->SetLayerFeatureType(SmtFtDot);
			   pLayer->SetLayerName(info.szName);

			   if (!pLayer->Open(info.szArchiveName))
			   {
				   SMT_SAFE_DELETE(pLayer);	
			   }
			   else
				   pLayer->Fetch();
		   }
		   break;
	   case SmtFtChildImage:
		   {
			   pLayer = new SmtCldImgFclsDBLayer(this);
			   pLayer->SetLayerFeatureType(SmtFtChildImage);
			   pLayer->SetLayerName(info.szName);

			   if (!pLayer->Open(info.szArchiveName))
			   {
				   SMT_SAFE_DELETE(pLayer);	
			   }
			   else
				   pLayer->Fetch();   
		   }
		   break;
	   case SmtFtAnno:
		   {
			   pLayer = new SmtAnnoFclsAdoLayer(this);
			   pLayer->SetLayerFeatureType(SmtFtAnno);
			   pLayer->SetLayerName(info.szName);

			   if (!pLayer->Open(info.szArchiveName))
			   {
				   SMT_SAFE_DELETE(pLayer);	
			   }
			   else
				   pLayer->Fetch();
		   }
		   break;

	   case SmtFtCurve:
		   {
			   pLayer = new SmtCurveFclsAdoLayer(this);
			   pLayer->SetLayerFeatureType(SmtFtCurve);
			   pLayer->SetLayerName(info.szName);

			   if (!pLayer->Open(info.szArchiveName))
			   {
				   SMT_SAFE_DELETE(pLayer);	
			   }
			   else
				   pLayer->Fetch();
		   }
		   break;
	   case SmtFtGrid:
		   {
			   pLayer = new SmtGridFclsAdoLayer(this);
			   pLayer->SetLayerFeatureType(SmtFtGrid);
			   pLayer->SetLayerName(info.szName);

			   if (!pLayer->Open(info.szArchiveName))
			   {
				   SMT_SAFE_DELETE(pLayer);	
			   }
			   else
				   pLayer->Fetch();
		   }
		   break;
	   case SmtFtTin:
		   {
			   pLayer = new SmtTinFclsAdoLayer(this);
			   pLayer->SetLayerFeatureType(SmtFtTin);
			   pLayer->SetLayerName(info.szName);

			   if (!pLayer->Open(info.szArchiveName))
			   {
				   SMT_SAFE_DELETE(pLayer);	
			   }
			   else
				   pLayer->Fetch();
		   }
		   break;
	   case SmtFtSurface:
		   {
			   pLayer = new SmtSurfaceFclsAdoLayer(this);
			   pLayer->SetLayerFeatureType(SmtFtSurface);
			   pLayer->SetLayerName(info.szName);

			   if (!pLayer->Open(info.szArchiveName))
			   {
				   SMT_SAFE_DELETE(pLayer);	
			   }
			   else
				   pLayer->Fetch();
		   }
		   break;
	  
	   default:
		   break;
	   }

	   return (SmtVectorLayer*)pLayer;
   }

   bool  SmtAdoDataSource::DeleteVectorLayer(const char *szName)
   {
	   char szSqlBuf[SQL_STRING_BUF_LENGTH];
	   sprintf(szSqlBuf,"select * from %s where %s = '%s';",DS_TABLE_NAME,DS_FLD_LYR_NAME,szName);
	  
	   if(!m_SmtRecordset.Open(szSqlBuf)) 
		   return false;

	   SmtLayerInfo info;
	   GetLayerTableInfo(info);

	   if (!m_SmtRecordset.Delete())
		   return false;

	   DropTable(info.szArchiveName);

       return true;
   }

   SmtRasterLayer* SmtAdoDataSource::CreateRasterLayer(const char *szName,fRect &lyrRect,long lImageCode)
   {
	   SmtAdoRasLayer *pLayer = NULL;
	   pLayer = new SmtAdoRasLayer(this);
	   pLayer->SetLayerName(szName);
	   pLayer->SetLayerRect(lyrRect);

	   if (!pLayer->Create())
		   SMT_SAFE_DELETE(pLayer);		

	   return (SmtRasterLayer*)pLayer;
   }

   SmtRasterLayer* SmtAdoDataSource::OpenRasterLayer(const char *szName)
   {
	  
	   SmtLayerInfo info;
	   if (!GetLayerTableInfoByLayerName(info,szName)) 
		   return NULL;
	  
	   SmtAdoRasLayer *pLayer = NULL;

	   pLayer = new SmtAdoRasLayer(this);
	   pLayer->SetLayerName(info.szName);

	   if (!pLayer->Open(info.szArchiveName))
	   {
		   SMT_SAFE_DELETE(pLayer);	
	   }
	   else
		   pLayer->Fetch();

	   return (SmtRasterLayer*)pLayer;
   }	

   bool SmtAdoDataSource::DeleteRasterLayer(const char *szName)
   {
	   char szSqlBuf[SQL_STRING_BUF_LENGTH];
	   sprintf(szSqlBuf,"select * from %s where %s = '%s';",DS_TABLE_NAME,DS_FLD_LYR_NAME,szName);

	   if(!m_SmtRecordset.Open(szSqlBuf)) 
		   return false;

	   SmtLayerInfo info;
	   GetLayerTableInfo(info);

	   if (!m_SmtRecordset.Delete())
		   return false;

	   DropTable(info.szArchiveName);

	   return true;
   }

   //////////////////////////////////////////////////////////////////////////
   	bool  SmtAdoDataSource::CreateDsTable(void)
	{
		bool bRet = TRUE;
		SmtAttribute smtAtt;
		SmtField     smtFld;

		smtFld.SetName(DS_FLD_LYR_NAME);
		smtFld.SetType(SmtVarType::SmtString);
		smtAtt.AddField(smtFld);

		smtFld.SetName(DS_FLD_LYR_TABLENAME);
		smtFld.SetType(SmtVarType::SmtString);
		smtAtt.AddField(smtFld);

		smtFld.SetName(DS_FLD_LYR_TYPE);
		smtFld.SetType(SmtVarType::SmtInteger);
		smtAtt.AddField(smtFld);

		smtFld.SetName(DS_FLD_LYR_SPIDXTYPE);
		smtFld.SetType(SmtVarType::SmtInteger);
		smtAtt.AddField(smtFld);

		smtFld.SetName(DS_FLD_LYR_XMIN);
		smtFld.SetType(SmtVarType::SmtReal);
		smtAtt.AddField(smtFld);

		smtFld.SetName(DS_FLD_LYR_YMIN);
		smtFld.SetType(SmtVarType::SmtReal);
		smtAtt.AddField(smtFld);

		smtFld.SetName(DS_FLD_LYR_XMAX);
		smtFld.SetType(SmtVarType::SmtReal);
		smtAtt.AddField(smtFld);

		smtFld.SetName(DS_FLD_LYR_YMAX);
		smtFld.SetType(SmtVarType::SmtReal);
		smtAtt.AddField(smtFld);

		bRet = CreateTable(DS_TABLE_NAME,&smtAtt);

		return bRet;
	}

	bool SmtAdoDataSource::GetLayerTableInfos(void)
	{
         if(!OpenTable( DS_TABLE_NAME,&m_SmtRecordset ))
			 return false;

		 m_vLayerInfos.clear();

		 m_SmtRecordset.MoveFirst();
		 while(!m_SmtRecordset.IsEOF())
		 {
			 SmtLayerInfo info;
			 GetLayerTableInfo(info);
			 m_vLayerInfos.push_back(info);
			 m_SmtRecordset.MoveNext();
		 }
		 
		 return true;
	}

	bool SmtAdoDataSource::GetLayerTableInfoByLayerName(SmtLayerInfo &info,const char * szName)
	{
		char szSqlBuf[SQL_STRING_BUF_LENGTH];
		sprintf_s(szSqlBuf,SQL_STRING_BUF_LENGTH,"select * from %s where %s = '%s';",DS_TABLE_NAME,DS_FLD_LYR_NAME,szName);
	
         if(!m_SmtRecordset.Open(szSqlBuf) || m_SmtRecordset.GetRecordCount() < 1) 
			 return false;

		 m_SmtRecordset.MoveFirst();
		 if (!m_SmtRecordset.IsEOF())
			 GetLayerTableInfo(info);

		 return true;
	}

	void SmtAdoDataSource::GetLayerTableInfo(SmtLayerInfo &info)
	{
		char  szStrBuf[TEMP_BUFFER_SIZE];
		int   ntype;   
		float fTemp; 
		m_SmtRecordset.GetCollect(DS_FLD_LYR_NAME,szStrBuf,TEMP_BUFFER_SIZE); 
		strcpy(info.szName,szStrBuf);

		m_SmtRecordset.GetCollect(DS_FLD_LYR_TABLENAME,szStrBuf,TEMP_BUFFER_SIZE); 
		strcpy(info.szArchiveName,szStrBuf);

		m_SmtRecordset.GetCollect(DS_FLD_LYR_TYPE,ntype);  
		info.unFeatureType = ntype;

		m_SmtRecordset.GetCollect(DS_FLD_LYR_SPIDXTYPE,ntype);  
		info.unSIType = ntype;

		m_SmtRecordset.GetCollect(DS_FLD_LYR_XMIN,fTemp);  
		info.lyrRect.lb.x = fTemp;

		m_SmtRecordset.GetCollect(DS_FLD_LYR_YMIN,fTemp);  
		info.lyrRect.lb.y = fTemp;

		m_SmtRecordset.GetCollect(DS_FLD_LYR_XMAX,fTemp);  
		info.lyrRect.rt.x = fTemp;

		m_SmtRecordset.GetCollect(DS_FLD_LYR_YMAX,fTemp);  
		info.lyrRect.rt.y = fTemp;
	}

	bool SmtAdoDataSource::AppendLayerTableInfo(SmtLayerInfo &info)
	{
		if(!OpenTable( DS_TABLE_NAME,&m_SmtRecordset ))
			return false;

		m_SmtRecordset.AddNew();

		m_SmtRecordset.PutCollect(DS_FLD_LYR_NAME,_variant_t(info.szName));
		m_SmtRecordset.PutCollect(DS_FLD_LYR_TABLENAME,_variant_t(info.szArchiveName));
		m_SmtRecordset.PutCollect(DS_FLD_LYR_TYPE,_variant_t((long)info.unFeatureType));
		m_SmtRecordset.PutCollect(DS_FLD_LYR_SPIDXTYPE,_variant_t((long)info.unSIType));
		m_SmtRecordset.PutCollect(DS_FLD_LYR_XMIN,_variant_t(info.lyrRect.lb.x));
		m_SmtRecordset.PutCollect(DS_FLD_LYR_YMIN,_variant_t(info.lyrRect.lb.y));
		m_SmtRecordset.PutCollect(DS_FLD_LYR_XMAX,_variant_t(info.lyrRect.rt.x));
		m_SmtRecordset.PutCollect(DS_FLD_LYR_YMAX,_variant_t(info.lyrRect.rt.x));

		if (!m_SmtRecordset.Update()) 
		{		 
			return false;
		}

		return true;
	}

	bool SmtAdoDataSource::DeleteLayerTableInfo(SmtLayerInfo &info)
	{
		if(!OpenTable( DS_TABLE_NAME,&m_SmtRecordset ))
			return false;
		return true;
	}

	bool SmtAdoDataSource::UpdateLayerTableInfo(SmtLayerInfo &info)
	{
		if(!OpenTable( DS_TABLE_NAME,&m_SmtRecordset ))
			return false;

		return true;
	}

	//////////////////////////////////////////////////////////////////////////
	bool  SmtAdoDataSource::CreateLayerTable(SmtLayerInfo &info,SmtAttribute   *pAtt)
	{
		bool bRet = false;
		if (CreateTable(info.szArchiveName,pAtt) && AppendLayerTableInfo(info))
			bRet = true;

		return bRet;	 
	}

   //////////////////////////////////////////////////////////////////////////
   bool  SmtAdoDataSource::CreateTable(const char * szTableName,SmtAttribute  *pAtt)
   {
	   bool bExist = IsTableExist(szTableName);
	   if(bExist)
		   return true;	

	   string sql = "create table ";
	   sql += szTableName;
	   sql += "( ";

	   int nCount = pAtt->GetFieldCount();

	   if (nCount < 1)
		   return false;

	   string str;
	   SmtField *pSmtFld = pAtt->GetFieldPtr(0);
	    CetTableFldCreatingString(str,*pSmtFld);

		str += " primary key ";

	   for (int i = 1; i < nCount; i++)
	   {
		   str += ",";
		   SmtField *pSmtFld = pAtt->GetFieldPtr(i);
		   CetTableFldCreatingString(str,*pSmtFld);
	   }

	   sql += str;
	   sql += ");";
	   return m_SmtConnection.Execute(sql.c_str());
   }

   void SmtAdoDataSource::CetTableFldCreatingString(string & str,SmtField &smtFld)
   {
       varType type = smtFld.GetType();
	   switch (type)
	   {
	   case   SmtInteger:
		   {
               str += smtFld.GetName();
			   str += " int ";
		   }
		      break;

	   case   SmtReal:	
		   {
			   str += smtFld.GetName();
			   str += " float ";
		   }
		      break;

	   case   SmtBool :	
		   {
			   str += smtFld.GetName();
			   str += " char(1) ";
		   }
		      break;

	   case   SmtByte :
		   {
			   str += smtFld.GetName();
			   str += " char(1) ";
		   }
		      break;

	   case   SmtString :
		   {
			   str += smtFld.GetName();
			   str += " varchar(200) ";
		   }
		      break;

	   case   SmtIntegerList:
		   {
			   str += smtFld.GetName();
			   str += " image ";
		   }
		      break;

	   case   SmtRealList:
		   {
			   str += smtFld.GetName();
			   str += " image ";
		   }
		      break;

	   case   SmtStringList:
		   {
			   str += smtFld.GetName();
			   str += " image ";
		   }
		      break;

	   case   SmtBinary:
		   {
			   str += smtFld.GetName();
			   str += " image ";
		   }
		      break;

	   case   SmtDate:
		   {
			   str += smtFld.GetName();
			   str += " image ";
		   }
		      break;
	   case   SmtTime:
		   {
			   str += smtFld.GetName();
			   str += " image ";
		   }
		      break;

	   case   SmtDateTime:
		   {
			   str += smtFld.GetName();
			   str += " image ";
		   }
		      break;

	   case   SmtUnknown://未知类型
		      break;

	   default:
		      break;
	   }
   }

   bool SmtAdoDataSource::OpenTable(const char * szTableName,SmtAdoRecordSet *pSmtRecordset,CursorTypeEnum CursorType, LockTypeEnum LockType)
   {
	   BOOL bRet = TRUE;
	   if (pSmtRecordset)
	   {
		   bRet = pSmtRecordset->Open(szTableName,adCmdTable,CursorType,LockType);
	   }
	   return ( (bRet == TRUE) ? true:false);
   }

   bool SmtAdoDataSource::CloseTable(const char * szTableName,SmtAdoRecordSet *pSmtRecordset)
   {
	   if (pSmtRecordset)
	   {
		   pSmtRecordset->Close();
	   }
	   return true;
   }

   bool SmtAdoDataSource::DropTable(const char * szTableName)
   {
	   BOOL bExist = IsTableExist(szTableName);
	   if(!bExist)
		   return true;
	   else 
	   {
		   char sql[TEMP_BUFFER_SIZE];
		   sprintf_s(sql,TEMP_BUFFER_SIZE,"drop table %s;",szTableName);

		   return m_SmtConnection.Execute(sql);
	   }
   }

   bool SmtAdoDataSource::DropTrigger(const char * szTriggleName)
   {
	   char sql[TEMP_BUFFER_SIZE];
	   sprintf_s(sql,TEMP_BUFFER_SIZE,"drop triggle %s;",szTriggleName);

	   return m_SmtConnection.Execute(sql);
   }

   bool SmtAdoDataSource::ClearTable(const char * szTableName)
   {
	   BOOL bExist = IsTableExist(szTableName);
	   if(!bExist)
		   return true;
	   else 
	   {
		   char sql[TEMP_BUFFER_SIZE];
		   sprintf_s(sql,TEMP_BUFFER_SIZE,"delete from %s;",szTableName);

		   return m_SmtConnection.Execute(sql);
	   }
   }

   bool SmtAdoDataSource::IsTableExist(const char * szTableName)
   {
	   if (m_SmtRecordset.IsOpen()) 
		   m_SmtRecordset.Close();

	   m_SmtRecordset = m_SmtConnection.OpenSchema(adSchemaTables);//枚举表的名称处理

	   m_SmtRecordset.MoveFirst();
	   while(!(m_SmtRecordset.IsEOF()))
	   {
		   char strTableName[TEMP_BUFFER_SIZE];

		   _bstr_t table_name = m_SmtRecordset.GetFields()->GetItem("TABLE_NAME")->Value;//获取表的名称

		   sprintf_s(strTableName,TEMP_BUFFER_SIZE,"%s",(LPCSTR) table_name);
		 
		   if(!strcmp(strTableName,szTableName))
		   {
			   return TRUE;
		   }

		   m_SmtRecordset.MoveNext();
	   }

	   return FALSE;
   }
}