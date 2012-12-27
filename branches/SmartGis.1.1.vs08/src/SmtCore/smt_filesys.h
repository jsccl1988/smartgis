/*
File:    smt_filesys.h

Desc:    SmartGis 文件系统头文件

Version: Version 1.0

Writter:  陈春亮

Date:    2011.8.2

Copyright (c) 2010 CCL. All rights reserved.
*/
#ifndef _SMT_FILESYSTEM_H
#define _SMT_FILESYSTEM_H
#include "smt_core.h"

namespace Smt_Core
{
	struct SmtFileInfo 
	{//file info
		char  szName[MAX_NAME_LENGTH];            
		char  szPath[MAX_FILE_PATH];            

		SmtFileInfo(void)
		{
			memset(szName,'\0',MAX_NAME_LENGTH); 
			memset(szPath,'\0',MAX_FILE_PATH);  
		}

		void operator =(const SmtFileInfo &other)
		{
			memset(szName,'\0',MAX_NAME_LENGTH);   
			memset(szPath,'\0',MAX_FILE_PATH);    
			strcpy(szName,other.szName);            
			strcpy(szPath,other.szPath);            
		}
	};

	typedef vector<SmtFileInfo> vSmtFileInfos;

	class SMT_EXPORT_CLASS SmtFileSystem
	{
	public:
		SmtFileSystem(void);
		virtual ~SmtFileSystem(void);

	public:
		inline  void            SetCurrentDir(const char * chCurrentDir){strcpy(m_zsCurrenDir,chCurrentDir);}
		inline  const char *    GetCurrentDir(void) const {return m_zsCurrenDir;}
		bool                    UpDir(int n);

		inline  vSmtFileInfos&  GetFileInfos(void){return  m_vFileInfos;}
		inline  const vSmtFileInfos&  GetFileInfos(void) const {return  m_vFileInfos;}


		virtual bool            SearchCurrentDir(const char* chFilter,bool bIsFindFolder = true);		

		void                    ClearFileInfos(void);
	
	protected:
		void                    ScanDir(const char * chDir,const char* chFilter, bool bIsFindFolder);

	protected:
		vSmtFileInfos			m_vFileInfos;
		char					m_zsCurrenDir[MAX_FILE_PATH];
	};
}


#if !defined(Export_SmtCore)
#if   defined( _DEBUG)
#          pragma comment(lib,"SmtCoreD.lib")
#       else
#          pragma comment(lib,"SmtCore.lib")
#	    endif  
#endif

#endif //_SMT_FILESYSTEM_H