#include "smt_assert.h"

bool SmtAssert(bool bContent,char *szDesc,int nLine,char *szFile,bool *pBIgnoreAlways)
{
	if (OpenClipboard(NULL))
	{
		HGLOBAL hMem;
		char szAssert[256];
		char *pMem = NULL;

		sprintf_s(szAssert,256,"Input assert info here");
		hMem = GlobalAlloc(GHND|GMEM_DDESHARE,strlen(szAssert)+1);
		if (hMem)
		{
			pMem = (char*)GlobalLock(hMem);
			strcpy(pMem,szAssert);
			GlobalUnlock(hMem);
			EmptyClipboard();
			SetClipboardData(CF_TEXT,hMem);
		}

		CloseClipboard();
	}

	int nMsg = MessageBox(NULL,szDesc,"SmtGis- «∑Ò÷–∂œ",IDOK);
	switch (nMsg)
	{
	case IDIGNORE:
		{
			*pBIgnoreAlways = true;
			return false;
		}
	case IDOK:
		{
			*pBIgnoreAlways = false;
			return true;
		}
		break;
	case IDCANCEL:
		{
			return false;
		}
		break;
	default:
		return false;
	}
	
	return false;
}