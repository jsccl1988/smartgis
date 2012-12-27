#include "d3d_3drenderdevice.h"
#include "d3d_text.h"
#include "smt_logmanager.h"

using namespace Smt_Core;

namespace Smt_3Drd
{
	//////////////////////////////////////////////////////////////////////////
	//print implement
	long SmtD3DRenderDevice::CreateFont(const char *szChType,int nHeight,int nWidth,int nWeight,bool bItalic,bool bUnderline,bool bStrike,ulong dwSize,uint &unID ) 
	{
		HRESULT		hr;
		ID3DXFont	*pFont = NULL;

		if(nHeight == 0 && nWidth == 0)
		{
			HDC hDC = GetDC( NULL );

			nHeight = MulDiv(dwSize, ::GetDeviceCaps(hDC, LOGPIXELSY), 72);
			//m_nWidth  = MulDiv(dwSize, GetDeviceCaps(hDC, LOGPIXELSX), 72);

			ReleaseDC( NULL, hDC );
		}

		hr = D3DXCreateFont( m_pD3DDevice,            // D3D device
			nHeight,               // Height
			nWidth,                // Width
			nWeight,               // Weight
			0,                     // MipLevels, 0 = autogen mipmaps
			bItalic,               // Italic
			DEFAULT_CHARSET,       // CharSet
			OUT_DEFAULT_PRECIS,    // OutputPrecision
			DEFAULT_QUALITY,       // Quality
			DEFAULT_PITCH | FF_DONTCARE, // PitchAndFamily
			szChType,				   // pFaceName
			&pFont );              // ppFont

		if( FAILED( hr ) )
			return SMT_ERR_FAILURE;

		if( FAILED( hr = D3DXCreateFont( m_pD3DDevice, nHeight,nWidth, nWeight,0 ,bItalic, DEFAULT_CHARSET,
			OUT_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE,
			"System", &pFont ) ) )
			return SMT_ERR_FAILURE;

		if( FAILED( hr = D3DXCreateFont( m_pD3DDevice, nHeight,nWidth, nWeight,0,bItalic,  DEFAULT_CHARSET,
			OUT_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE,
			"Arial", &pFont ) ) )
			return SMT_ERR_FAILURE;

		if (SUCCEEDED(hr)) 
		{
			m_vD3DFontPtrs.push_back(pFont);
			unID = m_vD3DFontPtrs.size()-1;
			return SMT_ERR_NONE;
		}

		return SMT_ERR_FAILURE;
	}
}