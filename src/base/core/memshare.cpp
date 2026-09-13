#include "base/core/memshare.h"

namespace base
{
	SmtMemShare::SmtMemShare(const char * szMapFile,int nFileSize,bool bServer):m_hFileMap(NULL)
		,m_pDataBuffer(NULL)
	{
		if (bServer)
		{
			//����һ���ڴ�ӳ���ļ�����
			m_hFileMap = CreateFileMapping(INVALID_HANDLE_VALUE,NULL,PAGE_READWRITE,0,nFileSize,szMapFile);
		}
		else
			//��һ���ڴ�ӳ���ļ�����
			m_hFileMap = OpenFileMapping(FILE_MAP_ALL_ACCESS,FALSE,szMapFile);

		//ӳ�䵽�����ڴ棬ȡ�ù����ڴ���׵�ַ
		m_pDataBuffer = (LPBYTE)MapViewOfFile(m_hFileMap,FILE_MAP_ALL_ACCESS,0,0,0);
	}

	SmtMemShare::~SmtMemShare()
	{
		//ȡ���ļ���ӳ�䣬�ر��ļ�ӳ�������
		UnmapViewOfFile(m_pDataBuffer);
		CloseHandle(m_hFileMap);
	}
}