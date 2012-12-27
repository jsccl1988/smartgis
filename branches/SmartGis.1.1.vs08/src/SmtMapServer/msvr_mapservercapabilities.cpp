#include "msvr_mapservercapabilities.h"
#include "smt_logmanager.h"
#include <iostream>
#include "smt_xml.h"

namespace Smt_MapServer
{
	SmtCapabilities::SmtCapabilities(SmtMapService *pMapService):m_pMapService(pMapService)
	{

	}

	SmtCapabilities::~SmtCapabilities()
	{
		m_doc.Clear();
	}

	long SmtCapabilities::Create(SmtMapService *pMapService)
	{
		if (NULL != pMapService)
			m_pMapService = pMapService;
		else if (NULL == m_pMapService)
			return SMT_ERR_FAILURE;

		m_doc.Clear();

		return SMT_ERR_NONE;
	}

	long SmtCapabilities::Print(FILE* cfile, int depth)
	{
		m_cslock.Lock();

		m_doc.Print(cfile,depth);

		m_cslock.Unlock();

		return SMT_ERR_NONE;
	}

	long SmtCapabilities::Save( const char * filename )
	{
		m_cslock.Lock();

		m_doc.SaveFile(filename);

		m_cslock.Unlock();

		return SMT_ERR_NONE;
	}

	long SmtCapabilities::GetXML(char *& pXMLBuf,long &lBufLength)
	{
		m_cslock.Lock();

		TiXmlPrinter printer;

		printer.SetStreamPrinting();
		m_doc.Accept( &printer );
		lBufLength = strlen(printer.CStr())+1;

		pXMLBuf = new char [lBufLength];

		memset(pXMLBuf,'\0',lBufLength);
		strcpy_s(pXMLBuf,lBufLength,printer.CStr());

		m_cslock.Unlock();

		return SMT_ERR_NONE;
	}

	long SmtCapabilities::FreeXMLBuf(char *& pXMLBuf)
	{
		SMT_SAFE_DELETE_A(pXMLBuf);

		return SMT_ERR_NONE;
	}
}