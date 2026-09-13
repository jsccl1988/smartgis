#include "base/style/style.h"
#include "base/style/stylemanager.h"

namespace base
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

	SmtStyleTable * SmtStyleTable::clone(void) const
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
	int SmtStyleTable::find_style_name_index(const char *stylename) const
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

	int SmtStyleTable::add_style(const char * stylename)
	{
		int index = find_style_name_index(stylename);
		if (index >0 && index < m_nStyleCount)
			return SMT_ERR_NONE;
		
		SmtStyleManager *pStyleMgr = SmtStyleManager::get_singleton_ptr();
		if (pStyleMgr->get_style(stylename) != NULL)
		{
			m_pStyleNames = (char **)realloc( m_pStyleNames, sizeof(void*)*(m_nStyleCount+1) );
            m_pStyleNames[m_nStyleCount] = strdup(stylename);
			m_nStyleCount++;

			return SMT_ERR_NONE;
		}

		return SMT_ERR_FAILURE;
	}

	void SmtStyleTable::remove_style(const char * stylename)
	{
		int index = find_style_name_index(stylename);
		if (index != -1 && index  < m_nStyleCount)
		{
			SMT_SAFE_DELETE_A(m_pStyleNames[index]);
			memmove( m_pStyleNames + index, m_pStyleNames + index + 1, sizeof(void*) * (m_nStyleCount-index-1) );
			m_nStyleCount--;
		}
	}

	SmtStyle *SmtStyleTable::get_style(const char * stylename)
	{
		 SmtStyleManager *pStyleMgr = SmtStyleManager::get_singleton_ptr();
		 int index = find_style_name_index(stylename);
		 if (index != -1)
		 {	
			 SmtStyle *pStyle = pStyleMgr->get_style(stylename) ;
			 if (NULL != pStyle)
				 return pStyle;
		 }
			 
		 return pStyleMgr->get_default_style();
	}

	const SmtStyle *SmtStyleTable::get_style(const char * stylename) const 
	{
		SmtStyleManager *pStyleMgr = SmtStyleManager::get_singleton_ptr();
		int index = find_style_name_index(stylename);
		if (index != -1)
		{	
			SmtStyle *pStyle = pStyleMgr->get_style(stylename) ;
			if (NULL != pStyle)
				return pStyle;
		}

		return pStyleMgr->get_default_style();
	}

	SmtStyle *SmtStyleTable::get_style(int index)
	{
		SmtStyleManager *pStyleMgr = SmtStyleManager::get_singleton_ptr();
		if (index > -1 && index < m_nStyleCount)
		{
			SmtStyle *pStyle = pStyleMgr->get_style(m_pStyleNames[index]) ;
			if (pStyle!= NULL)
				return pStyle;
		}	

		return pStyleMgr->get_default_style();
	}

	const SmtStyle *SmtStyleTable::get_style(int index) const
	{
		SmtStyleManager *pStyleMgr = SmtStyleManager::get_singleton_ptr();
		if (index > -1 && index < m_nStyleCount)
		{
			SmtStyle *pStyle = pStyleMgr->get_style(m_pStyleNames[index]) ;
			if (pStyle!= NULL)
				return pStyle;
		}	

		return pStyleMgr->get_default_style();
	}

	const char *SmtStyleTable::get_style_name(int index)
	{
		if (m_pStyleNames && index != -1 && index  < m_nStyleCount)
			return m_pStyleNames[index];
		else
			return "";
	}

}