/*
File:    smt_filesys.h

Desc:    SmartGis �ļ�ϵͳͷ�ļ�

Version: Version 1.0

Writter:  �´���

Date:    2011.8.2

Copyright (c) 2010 CCL. All rights reserved.
*/
#ifndef _SMT_FILESYSTEM_H
#define _SMT_FILESYSTEM_H
#include "base/core/core.h"

namespace base
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

	class CORE_EXPORT SmtFileSystem
	{
	public:
		SmtFileSystem(void);
		virtual ~SmtFileSystem(void);

	public:
		inline  void            set_current_dir(const char * chCurrentDir){strcpy(m_zsCurrenDir,chCurrentDir);}
		inline  const char *    get_current_dir(void) const {return m_zsCurrenDir;}
		bool                    up_dir(int n);

		inline  vSmtFileInfos&  get_file_infos(void){return  m_vFileInfos;}
		inline  const vSmtFileInfos&  get_file_infos(void) const {return  m_vFileInfos;}


		virtual bool            search_current_dir(const char* chFilter,bool bIsFindFolder = true);		

		void                    clear_file_infos(void);
	
	protected:
		void                    scan_dir(const char * chDir,const char* chFilter, bool bIsFindFolder);

	protected:
		vSmtFileInfos			m_vFileInfos;
		char					m_zsCurrenDir[MAX_FILE_PATH];
	};
}


#if !defined(CORE_EXPORTS)
#if   defined( _DEBUG)
#          pragma comment(lib,"base_d.lib")
#       else
#          pragma comment(lib,"base.lib")
#	    endif  
#endif

#endif //_SMT_FILESYSTEM_H