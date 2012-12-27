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
		HANDLE hFind = ::FindFirstFile(szPath, &fileFindData);            //�ҵ���һ��

		if (hFind == INVALID_HANDLE_VALUE)       
		{ //���û���ҵ���ص��ļ���Ϣ������false
			return;
		}

		do 
		{//����֮�������һ����ֱ��������
			if (fileFindData.cFileName[0] == '.')
			{//��Ϊ�ļ��п�ʼ��"."��".."����Ŀ¼��Ҫ���˵�
				continue;            
			}

			//�ļ�������·��
			sprintf_s(szPath, MAX_PATH, "%s%s", chDir, fileFindData.cFileName); 

			//���Ҫ���ҵݹ�����ļ���
			if (bIsFindFolder && (fileFindData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
			{//�ݹ����������������Ŀ¼
				ScanDir(szPath, chFilter, bIsFindFolder);            
			}

			//������ļ�
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

		FindClose(hFind);                //�رղ��Ҿ��
	}
#else
	void  SmtFileSystem::ScanDir(const char * chDir,const char* chFilter, bool bIsFindFolder)
	{
		char* szPath[MAX_PATH];
		sprintf_s(szPath, MAX_PATH, "%s%s", chDir, chFilter);

		_finddata_t fileFindData;
		long lf;
		if((lf = _findfirst(pChPath,&fileFindData))==-1)
		{//�ļ�û���ҵ�!
			return ;
		}

		do 
		{//����֮�������һ����ֱ��������
			if ( fileFindData.name[0]== '.')
			{//��Ϊ�ļ��п�ʼ��"."��".."����Ŀ¼��Ҫ���˵�
				continue;            
			}

			//�ļ�������·��
			sprintf_s(szPath, MAX_PATH, "%s%s", chDir, fileFindData.name); 

			//���Ҫ���ҵݹ�����ļ���
			if (bIsFindFolder && (fileFindData.attrib & _A_SUBDIR))
			{//�ݹ����������������Ŀ¼
				ScanDir(szPath, chFilter, bIsFindFolder);            
			}

			//������ļ�
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

		_findclose(lf);                //�رղ��Ҿ��
	}
#endif // SMT_USE_WINAPI

}