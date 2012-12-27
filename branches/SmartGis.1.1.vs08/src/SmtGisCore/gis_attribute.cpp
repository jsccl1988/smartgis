#include "gis_attribute.h"
#include "smt_logmanager.h"

namespace Smt_GIS
{
	SmtAttribute::SmtAttribute(void)
	{
       m_pFieldPtrs = NULL;
	   m_lFieldCount = 0;
	}

	SmtAttribute:: ~SmtAttribute(void)
	{
      for (int i = 0; i < m_lFieldCount ; i ++)
      {
		  SMT_SAFE_DELETE(m_pFieldPtrs[i]);
      }

	  free(m_pFieldPtrs);
	}

	//////////////////////////////////////////////////////////////////////////
	SmtAttribute *SmtAttribute::Clone(void) const
	{
        SmtAttribute *pNewSmtAtt = new SmtAttribute();
		if ( NULL == pNewSmtAtt)
			return NULL;

		if (m_lFieldCount < 1)
			return pNewSmtAtt;

        pNewSmtAtt->m_lFieldCount = m_lFieldCount;
		pNewSmtAtt->m_pFieldPtrs = new SmtField*[m_lFieldCount];
		for (int i = 0 ; i < m_lFieldCount ; i++)
		{
			//pNewSmtAtt->AddField(*(m_pFieldPtrs[i]));
			pNewSmtAtt->m_pFieldPtrs[i] = new SmtField();
            pNewSmtAtt->m_pFieldPtrs[i]->SetType(m_pFieldPtrs[i]->GetType());
			pNewSmtAtt->m_pFieldPtrs[i]->SetValue(*m_pFieldPtrs[i]->GetValuePtr());
            pNewSmtAtt->m_pFieldPtrs[i]->SetName(m_pFieldPtrs[i]->GetName());
		}

		return pNewSmtAtt;
	}

	//////////////////////////////////////////////////////////////////////////
	void SmtAttribute::AddField(SmtField & fld)
	{
		if (IsFieldExsit(fld.GetName()))
			return;

		m_pFieldPtrs = (SmtField **)realloc( m_pFieldPtrs, sizeof(SmtField *)*(m_lFieldCount+1) );
		m_pFieldPtrs[m_lFieldCount] = fld.Clone();
		m_lFieldCount++;
	}

	void SmtAttribute::RemoveField(const char * fldname)
	{
		int index = GetFieldIndex(fldname);
		if (index != -1 && index  < m_lFieldCount)
		{
			SMT_SAFE_DELETE(m_pFieldPtrs[index]);
			memmove( m_pFieldPtrs + index, m_pFieldPtrs + index + 1, sizeof(void*) * (m_lFieldCount-index-1) );
			m_lFieldCount--;
		}
	}

	//////////////////////////////////////////////////////////////////////////
	int SmtAttribute::GetFieldIndex(const char *fldname) const 
	{
		int index = -1;
		int i = 0;
		bool flag = false;
		while (!flag && i < m_lFieldCount)
		{
			if(strcmp(m_pFieldPtrs[i]->GetName(),fldname) == 0)
			{
				flag = true;
				index = i;
			}
			i++;
		}

		return index;
	}

	bool SmtAttribute::IsFieldExsit(const char *fldname) const 
	{
		int i = 0;
		bool flag = false;
		while (!flag && i < m_lFieldCount)
		{
           if(strcmp(m_pFieldPtrs[i]->GetName(),fldname) == 0)
			   flag = true;
		   i++;
		}
		return flag;
	}

}