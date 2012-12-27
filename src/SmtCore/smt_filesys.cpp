#include "smt_filesys.h"
#include "smt_api.h"
#include<io.h>

#define  SMT_USE_WINAPI 

namespace Smt_Core
{
	SmtFileSystem::SmtFileSystem(void)
	{
        ClearFileInfos();
	}

	SmtFileSystem::~SmtFileSystem(void)
	{
        ClearFileInfos();
	}

	void SmtFileSystem::ClearFileInfos(void)
	{
        m_vFileInfos.clear();
	}

	bool SmtFileSystem::UpDir(int n)
	{
		int i = n;
		string strNewPath = m_zsCurrenDir;
		int nPos;
		while (i)
		{
			nPos = strNewPath.rfind("\\");
			if (string::npos == nPos)
				return false;
			strNewPath = strNewPath.substr(0,nPos);
			i --;
		}

		strcpy(m_zsCurrenDir, strNewPath.c_str());

		return true;
	}

	bool  SmtFileSystem::SearchCurrentDir(const char* chFilter,bool bIsFindFolder)
	{
		ClearFileInfos();
        ScanDir(m_zsCurrenDir,chFilter,bIsFindFolder);
		return true;
	}

#ifdef SMT_USE_WINAPI
	void  SmtFileSystem::ScanDir(const char * chDir,const char* chFilter, bool bIsFindFolder)
	{
		char szPath[MAX_PATH];
		sprintf_s(szPath, MAX_PATH, "%s%s", chDir, chFilter);
		WIN32_FIND_DATA fileFindData;
		HANDLE hFind = ::FindFirstFile(szPath, &fileFindData);            //找到第一个

		if (hFind == INVALID_HANDLE_VALUE)       
		{ //如果没有找到相关的文件信息，返回false
			return;
		}

		do 
		{//处理之后查找下一个，直到都找完
			if (fileFindData.cFileName[0] == '.')
			{//因为文件夹开始有"."和".."两个目录，要过滤掉
				continue;            
			}

			//文件的完整路径
			sprintf_s(szPath, MAX_PATH, "%s%s", chDir, fileFindData.cFileName); 

			//如果要查找递归查找文件夹
			if (bIsFindFolder && (fileFindData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
			{//递归调用来查找他的子目录
				ScanDir(szPath, chFilter, bIsFindFolder);            
			}

			//如果是文件
			char path[_MAX_PATH];
			char fileName[_MAX_PATH];
			char title[_MAX_PATH];
			char ext[_MAX_PATH];

			SplitFileName(szPath,path,fileName,title,ext);

			SmtFileInfo info;
			strcpy(info.szName,fileName);
			strcpy(info.szPath,path);
			m_vFileInfos.push_back(info);

		}while (::FindNextFile(hFind, &fileFindData)); 

		FindClose(hFind);                //关闭查找句柄
	}
#else
	void  SmtFileSystem::ScanDir(const char * chDir,const char* chFilter, bool bIsFindFolder)
	{
		char* szPath[MAX_PATH];
		sprintf_s(szPath, MAX_PATH, "%s%s", chDir, chFilter);

		_finddata_t fileFindData;
		long lf;
		if((lf = _findfirst(pChPath,&fileFindData))==-1)
		{//文件没有找到!
			return ;
		}

		do 
		{//处理之后查找下一个，直到都找完
			if ( fileFindData.name[0]== '.')
			{//因为文件夹开始有"."和".."两个目录，要过滤掉
				continue;            
			}

			//文件的完整路径
			sprintf_s(szPath, MAX_PATH, "%s%s", chDir, fileFindData.name); 

			//如果要查找递归查找文件夹
			if (bIsFindFolder && (fileFindData.attrib & _A_SUBDIR))
			{//递归调用来查找他的子目录
				ScanDir(szPath, chFilter, bIsFindFolder);            
			}

			//如果是文件
			char path[_MAX_PATH];
			char fileName[_MAX_PATH];
			char title[_MAX_PATH];
			char ext[_MAX_PATH];

			SplitFileName(szPath,path,fileName,title,ext);

			SmtFileInfo info;
			strcpy(info.szName,fileName);
			strcpy(info.szPath,path);
			m_vFileInfos.push_back(info);

		}while (_findnext( lf, &fileFindData ) == 0); 

		_findclose(lf);                //关闭查找句柄
	}
#endif // SMT_USE_WINAPI

}