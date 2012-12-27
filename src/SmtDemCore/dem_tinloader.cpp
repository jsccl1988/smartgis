#include "dem_tinloader.h"
#include "tin_delaunay_incremental.h"
#include "tin_delaunay_divide.h"

using namespace Smt_TinMesh;

namespace Smt_DEM
{
	Smt3DTinLoader::Smt3DTinLoader():m_fXScale(1)
		,m_fYScale(1)
		,m_fZScale(1)
	{
		;
	}

	Smt3DTinLoader::~Smt3DTinLoader()
	{
		
	}

	//////////////////////////////////////////////////////////////////////////
	long Smt3DTinLoader::LoadDataFromASSII(const char * szFileName,SmtTinFileFmt &fileFmt)
	{
		fstream fin;
		locale loc1 = locale::global(locale(".936"));
		fin.open(szFileName,ios::in);
		locale::global(locale(loc1));

		if (!fin.is_open())
			return SMT_ERR_INVALID_FILE;

		char				szBuf[2000];
		string				strParseFmt = "";
		Vector3				ver;
		vector<Vector3>		vVers;

		//////////////////////////////////////////////////////////////////////////
		//跳过头
		int nHeadSkip = 0;
		while(!fin.eof() && nHeadSkip < fileFmt.nHeadSkip)
		{
			fin.getline(szBuf,2000,'\n');
			nHeadSkip++;
		}

		//////////////////////////////////////////////////////////////////////////
		//构造解析表达式
		char chSeparator = ',';
		switch (fileFmt.nSeparatorType)
		{
			case ST_TAB:
				chSeparator = '\t';
				break;
			case ST_SPACE:
				chSeparator = ' ';
				break;
			case ST_COMMA:
				chSeparator = ',';
				break;
			default:
				chSeparator = ',';
				break;
		}

		string strFloat = "%f",strFilter = "%*[^";
		strFloat  += chSeparator;
		strFilter += chSeparator;
		strFilter += "]";
		strFilter += chSeparator;

		for (int i = 0; i < fileFmt.nCol ;i++ )
		{
			if (i == fileFmt.iX || i == fileFmt.iY || i == fileFmt.iZ)
				strParseFmt += strFloat;
			else
				strParseFmt += strFilter;
		}

		//////////////////////////////////////////////////////////////////////////
		//读取
		while(!fin.eof())
		{
			//跳过
			int nLineSkip = fileFmt.nLineSkip+1;
			while(!fin.eof() && nLineSkip > 0)
			{
				fin.getline(szBuf,2000,'\n');
				nLineSkip--;
			}
			
			if (sscanf(szBuf,strParseFmt.c_str(),&ver.x,&ver.y,&ver.z) != 3)
				if (sscanf(szBuf,"%*[^,],,%f,%f,%f",&ver.x,&ver.y,&ver.z) != 3)
					continue;
			
			ver.x *= m_fXScale;
			ver.y *= m_fYScale;
			ver.z *= m_fZScale;

			vVers.push_back(ver);
		}

		fin.close();

		if (vVers.size() < 3)
			return SMT_ERR_INVALID_FILE;
		 
		//
		SmtTinDelaunayDiv tinDela;
		tinDela.SetVertexs(vVers);
	
		if (SMT_ERR_NONE == tinDela.DoMesh() &&
			SMT_ERR_NONE == tinDela.CvtTo3DSurf(&m_tinSurf))
		{
			m_fMinX = tinDela.GetMinX();
			m_fMinY = tinDela.GetMinY();
			m_fMinZ = tinDela.GetMinZ();

			m_fMaxX = tinDela.GetMaxX();
			m_fMaxY = tinDela.GetMaxY();
			m_fMaxZ = tinDela.GetMaxZ();

			return SMT_ERR_NONE;
		}

		return SMT_ERR_INVALID_FILE;
	}
}