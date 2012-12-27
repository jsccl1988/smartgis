#include "dem_gridloader.h"

namespace Smt_DEM
{
	Smt3DGridLoader::Smt3DGridLoader()
	{
		m_nY		= 0;
		m_nX        = 0;
		m_fXStart	= 0.;
		m_fYStart	= 0.;
		m_fZStart	= 0.;
		m_pData		= NULL;
	}

	Smt3DGridLoader::~Smt3DGridLoader()
	{
		SMT_SAFE_DELETE_A(m_pData);
		m_nY = 0;
		m_nX  = 0;
	}

	//////////////////////////////////////////////////////////////////////////
	long Smt3DGridLoader::LoadDataFromHeightMap(const char * szFileName)
	{
		fstream fin;
		locale loc1 = locale::global(locale(".936"));
		fin.open(szFileName,ios::binary|ios::in);
		locale::global(locale(loc1));

		if (!fin.is_open())
			return SMT_ERR_INVALID_FILE;

		if (m_pData)
		{
			SMT_SAFE_DELETE_A(m_pData);
			m_nY = 0;
			m_nX = 0;
		}

		BITMAPFILEHEADER bmfh;
		BITMAPINFOHEADER bmih;

		fin.read((char*)&bmfh, sizeof(bmfh));
		fin.read((char*)&bmih, sizeof(bmih));
		if(bmih.biBitCount != 8)
		{
			fin.close();
			return SMT_ERR_INVALID_FILE;
		}

		fin.seekg(bmfh.bfOffBits);

		m_nY = bmih.biHeight;
		m_nX = bmih.biWidth;
		int nBufSize = m_nY * m_nX;

		m_pData = new unsigned char[nBufSize];

		if (NULL != m_pData)
			fin.read((char *)m_pData,nBufSize);

		fin.close();

		return CreateGridSurf();
	}

	long Smt3DGridLoader::CreateGridSurf(void)
	{
		int nDots = m_nY * m_nX;
		int nTris = (m_nY -1)*(m_nX-1)*2;

		Smt3DPoint	  *p3DPointBuf = new Smt3DPoint[nDots];
		Smt3DTriangle *p3DTriBuf = new Smt3DTriangle[nTris];

		float	fX,fY,fZ;
		int		nFirstXIndex = 0;
		int		nFirstXIndexUp = 0;
		for(int  iY=0; iY< m_nY; iY++ )
		{
			nFirstXIndex = (iY*m_nX);
			fY = iY*m_fYScale + m_fYStart;
			for( int iX=0; iX< m_nX; iX++ )
			{
				fX = iX*m_fXScale + m_fXStart;

				fZ = GetScaledZAtIndex( iX, iY );

				if( fZ>m_fMaxZ )
					m_fMaxZ= fZ;

				else if( fZ<m_fMinZ ) 
					m_fMinZ= fZ;

				p3DPointBuf[nFirstXIndex+iX].SetX(fX);
				p3DPointBuf[nFirstXIndex+iX].SetY(fY);
				p3DPointBuf[nFirstXIndex+iX].SetZ(fZ);
			}
		}
		
		int nTriIndex = 0;
		for(int  iY=0; iY< m_nY-1; iY++ )
		{
			nFirstXIndex = iY*m_nX;
			nFirstXIndexUp = (iY+1)*m_nX;

			for( int iX=0; iX< m_nX - 1; iX++ )
			{
				p3DTriBuf[nTriIndex].a = nFirstXIndex+iX;
				p3DTriBuf[nTriIndex].b = nFirstXIndex+iX+1;
				p3DTriBuf[nTriIndex].c = nFirstXIndexUp+iX+1;

				nTriIndex++;

				p3DTriBuf[nTriIndex].a = nFirstXIndex+iX;
				p3DTriBuf[nTriIndex].b = nFirstXIndexUp+iX+1;
				p3DTriBuf[nTriIndex].c = nFirstXIndexUp+iX;

				nTriIndex++;
			}
		}

		m_gridSurf.AddPointCollection(p3DPointBuf,nDots);
		m_gridSurf.AddTriangleCollection(p3DTriBuf,nTris);

		SMT_SAFE_DELETE_A(p3DTriBuf);
		SMT_SAFE_DELETE_A(p3DPointBuf);

		return SMT_ERR_NONE;
	}
}