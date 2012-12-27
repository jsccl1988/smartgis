#include "bl_style.h"
#include "bl_stylemanager.h"

namespace Smt_Base
{
	SmtStyleTable::SmtStyleTable(void)
	{
	   m_nStyleCount = 0;
       m_pStyleNames = NULL;
	}

	SmtStyleTable::~SmtStyleTable(void)
	{
		int index = 0;
		while (index < m_nStyleCount)
		{
			free(m_pStyleNames[index]);
			index ++;
		}

		free(m_pStyleNames);

		m_nStyleCount = 0;
	}

	SmtStyleTable * SmtStyleTable::Clone(void) const
	{
        SmtStyleTable *pStyleTable = new SmtStyleTable();
		if (pStyleTable == NULL)
			return NULL;

		if (m_nStyleCount < 1)
			return NULL;

		pStyleTable->m_pStyleNames = new char*[m_nStyleCount];

		int i = 0;
		while (i<m_nStyleCount)
		{
			pStyleTable->m_pStyleNames[i] = strdup(m_pStyleNames[i]);
			i++;
		}

		pStyleTable->m_nStyleCount = m_nStyleCount;

		return pStyleTable;
	}

	//////////////////////////////////////////////////////////////////////////
	int SmtStyleTable::FindStyleNameIndex(const char *stylename) const
	{
		int index = 0;
		while (index < m_nStyleCount)
		{
			if (strcmp(m_pStyleNames[index],stylename) == 0)
				return index;
			index ++;
		}

		return -1;
	}

	int SmtStyleTable::AddStyle(const char * stylename)
	{
		int index = FindStyleNameIndex(stylename);
		if (index >0 && index < m_nStyleCount)
			return SMT_ERR_NONE;
		
		SmtStyleManager *pStyleMgr = SmtStyleManager::GetSingletonPtr();
		if (pStyleMgr->GetStyle(stylename) != NULL)
		{
			m_pStyleNames = (char **)realloc( m_pStyleNames, sizeof(void*)*(m_nStyleCount+1) );
            m_pStyleNames[m_nStyleCount] = strdup(stylename);
			m_nStyleCount++;

			return SMT_ERR_NONE;
		}

		return SMT_ERR_FAILURE;
	}

	void SmtStyleTable::RemoveStyle(const char * stylename)
	{
		int index = FindStyleNameIndex(stylename);
		if (index != -1 && index  < m_nStyleCount)
		{
			SMT_SAFE_DELETE_A(m_pStyleNames[index]);
			memmove( m_pStyleNames + index, m_pStyleNames + index + 1, sizeof(void*) * (m_nStyleCount-index-1) );
			m_nStyleCount--;
		}
	}

	SmtStyle *SmtStyleTable::GetStyle(const char * stylename)
	{
		 SmtStyleManager *pStyleMgr = SmtStyleManager::GetSingletonPtr();
		 int index = FindStyleNameIndex(stylename);
		 if (index != -1)
		 {	
			 SmtStyle *pStyle = pStyleMgr->GetStyle(stylename) ;
			 if (NULL != pStyle)
				 return pStyle;
		 }
			 
		 return pStyleMgr->GetDefaultStyle();
	}

	const SmtStyle *SmtStyleTable::GetStyle(const char * stylename) const 
	{
		SmtStyleManager *pStyleMgr = SmtStyleManager::GetSingletonPtr();
		int index = FindStyleNameIndex(stylename);
		if (index != -1)
		{	
			SmtStyle *pStyle = pStyleMgr->GetStyle(stylename) ;
			if (NULL != pStyle)
				return pStyle;
		}

		return pStyleMgr->GetDefaultStyle();
	}

	SmtStyle *SmtStyleTable::GetStyle(int index)
	{
		SmtStyleManager *pStyleMgr = SmtStyleManager::GetSingletonPtr();
		if (index > -1 && index < m_nStyleCount)
		{
			SmtStyle *pStyle = pStyleMgr->GetStyle(m_pStyleNames[index]) ;
			if (pStyle!= NULL)
				return pStyle;
		}	

		return pStyleMgr->GetDefaultStyle();
	}

	const SmtStyle *SmtStyleTable::GetStyle(int index) const
	{
		SmtStyleManager *pStyleMgr = SmtStyleManager::GetSingletonPtr();
		if (index > -1 && index < m_nStyleCount)
		{
			SmtStyle *pStyle = pStyleMgr->GetStyle(m_pStyleNames[index]) ;
			if (pStyle!= NULL)
				return pStyle;
		}	

		return pStyleMgr->GetDefaultStyle();
	}

	const char *SmtStyleTable::GetStyleName(int index)
	{
		if (m_pStyleNames && index != -1 && index  < m_nStyleCount)
			return m_pStyleNames[index];
		else
			return "";
	}

}