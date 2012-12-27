#include "smt_memshare.h"

namespace Smt_Core
{
	SmtMemShare::SmtMemShare(const char * szMapFile,int nFileSize,bool bServer):m_hFileMap(NULL)
		,m_pDataBuffer(NULL)
	{
		if (bServer)
		{
			//创建一个内存映射文件对象
			m_hFileMap = CreateFileMapping(INVALID_HANDLE_VALUE,NULL,PAGE_READWRITE,0,nFileSize,szMapFile);
		}
		else
			//打开一个内存映射文件对象
			m_hFileMap = OpenFileMapping(FILE_MAP_ALL_ACCESS,FALSE,szMapFile);

		//映射到它的内存，取得共享内存的首地址
		m_pDataBuffer = (LPBYTE)MapViewOfFile(m_hFileMap,FILE_MAP_ALL_ACCESS,0,0,0);
	}

	SmtMemShare::~SmtMemShare()
	{
		//取消文件的映射，关闭文件映射对象句柄
		UnmapViewOfFile(m_pDataBuffer);
		CloseHandle(m_hFileMap);
	}
}