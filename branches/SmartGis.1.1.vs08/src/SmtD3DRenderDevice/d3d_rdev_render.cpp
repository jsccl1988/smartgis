#include "d3d_3drenderdevice.h"
#include "d3d_indexbuffer.h"
#include "d3d_vertexbuffer.h"
#include "smt_logmanager.h"
#include "d3d_text.h"

using namespace Smt_Core;

namespace Smt_3Drd
{
	//////////////////////////////////////////////////////////////////////////
	// Rendering functions
	long SmtD3DRenderDevice::BeginRender()
	{
		m_pD3DDevice->BeginScene();
		m_pD3DDevice->SetTransform(D3DTS_VIEW, &m_viewD3DMatrix);

		return SMT_ERR_NONE;
	}

	long SmtD3DRenderDevice::EndRender()
	{
		m_pD3DDevice->EndScene();

		return SMT_ERR_NONE;
	}

	long SmtD3DRenderDevice::SwapBuffers()
	{
		m_pD3DDevice->Present( NULL, NULL, NULL, NULL );

		return SMT_ERR_NONE;
	}

	long SmtD3DRenderDevice::DrawPrimitives( PrimitiveType primitiveType, SmtVertexBuffer* pVB, DWORD baseVertex, DWORD primitiveCount )
	{
		if ( pVB==NULL )
			return 0;

		// Convert primitive type
		D3DPRIMITIVETYPE PT;
		ulong count;
		if ( SMT_ERR_NONE != GetD3DPrimitiveType( primitiveType, primitiveCount, &PT, &count ) )
			return SMT_ERR_FAILURE;

		// Say that the VB will be the source for our draw primitive calls
		if ( SMT_ERR_NONE != pVB->PrepareForDrawing() )
			return SMT_ERR_FAILURE;

		// Draw primitives
		if ( FAILED( m_pD3DDevice->DrawPrimitive( PT, baseVertex, primitiveCount ) ) )
			return SMT_ERR_FAILURE;

		return SMT_ERR_NONE;
	}

	long SmtD3DRenderDevice::DrawIndexedPrimitives( PrimitiveType primitiveType, SmtVertexBuffer *pVB,SmtIndexBuffer *pIB,ulong baseIndex,ulong primitiveCount )
	{
		if ( pVB==NULL || pIB == NULL )
			return SMT_ERR_INVALID_PARAM;

		// Convert primitive type
		D3DPRIMITIVETYPE PT;
		ulong count;
		if ( SMT_ERR_NONE != GetD3DPrimitiveType( primitiveType, primitiveCount, &PT, &count ) )
			return SMT_ERR_FAILURE;

		// Say that the VB will be the source for our draw primitive calls
		//--
		if (SMT_ERR_NONE != pVB->PrepareForDrawing() ||
			SMT_ERR_NONE != pIB->PrepareForDrawing())
			return SMT_ERR_FAILURE;

		// Draw primitives
		if ( FAILED( m_pD3DDevice->DrawIndexedPrimitive( PT, baseIndex,0,0,pIB->GetIndexCount() ,primitiveCount ) ) )
			return SMT_ERR_FAILURE;

		if ( SMT_ERR_NONE != pVB->EndDrawing() ||
			SMT_ERR_NONE != pIB->EndDrawing() )
			return SMT_ERR_FAILURE;

		return SMT_ERR_NONE;
	}

	inline long SmtD3DRenderDevice::GetD3DPrimitiveType( const PrimitiveType pt,const ulong nInitialPrimitiveCount,D3DPRIMITIVETYPE* D3DPrimitiveType,ulong* nD3DPrimitiveCount )
	{
		switch (pt)
		{
		case PT_POINTLIST:
			*D3DPrimitiveType = D3DPT_POINTLIST;
			*nD3DPrimitiveCount = nInitialPrimitiveCount;

			return SMT_ERR_NONE;

		case PT_LINELIST:
			*D3DPrimitiveType = D3DPT_LINELIST;
			*nD3DPrimitiveCount = 2*nInitialPrimitiveCount;

			return SMT_ERR_NONE;

		case PT_LINESTRIP:
			*D3DPrimitiveType = D3DPT_LINESTRIP;
			*nD3DPrimitiveCount = nInitialPrimitiveCount;

			return SMT_ERR_NONE;

		case PT_TRIANGLELIST:
			*D3DPrimitiveType = D3DPT_TRIANGLELIST;
			*nD3DPrimitiveCount = 3*nInitialPrimitiveCount;

			return SMT_ERR_NONE;

		case PT_TRIANGLESTRIP:
			*D3DPrimitiveType = D3DPT_TRIANGLESTRIP;
			*nD3DPrimitiveCount = nInitialPrimitiveCount+2;

			return SMT_ERR_NONE;

		case PT_TRIANGLEFAN:
			*D3DPrimitiveType = D3DPT_TRIANGLEFAN;
			*nD3DPrimitiveCount = nInitialPrimitiveCount+2;

			return SMT_ERR_NONE;

		default:
			*D3DPrimitiveType = D3DPT_FORCE_DWORD;
			*nD3DPrimitiveCount = nInitialPrimitiveCount;

			return SMT_ERR_NONE;
		}

		return SMT_ERR_FAILURE;
	}

	long SmtD3DRenderDevice::DrawText(UINT nID,float x, float y,float z,const SmtColor& color,const char *str, ...) 
	{
		if(strlen(str) < 1 ||
			nID > m_vD3DFontPtrs.size() - 1)	
			return SMT_ERR_FAILURE;

		ID3DXFont* pD3DFont = m_vD3DFontPtrs[nID];

		if(NULL == pD3DFont)
			return SMT_ERR_FAILURE;

		RECT rc = { x, y, 0, 0 };
		char text[256];
		memset(text,'\0',256);
		va_list args;

		va_start(args, str);
		vsprintf(text, str, args);
		va_end(args);

		m_pTextSprite->Begin( D3DXSPRITE_ALPHABLEND | D3DXSPRITE_SORT_TEXTURE );

		SetRect( &rc, x, y, 0, 0 );
		//pD3DFont->DrawText(NULL,text, -1, &rc, DT_SINGLELINE, D3DCOLOR_COLORVALUE(clr.fRed,clr.fGreen,clr.fBlue,clr.fA));
		pD3DFont->DrawText(m_pTextSprite,text, -1, &rc, DT_SINGLELINE, D3DCOLOR_COLORVALUE(color.fRed,color.fGreen,color.fBlue,color.fA));

		m_pTextSprite->End();

		return SMT_ERR_NONE;
	}

	long SmtD3DRenderDevice::DrawText(UINT nID,float x, float y,const SmtColor& color,const char *str, ...) 
	{
		if(strlen(str) < 1 ||
			nID > m_vD3DFontPtrs.size() - 1)	
			return SMT_ERR_FAILURE;

		ID3DXFont* pD3DFont = m_vD3DFontPtrs[nID];

		if(NULL == pD3DFont)
			return SMT_ERR_FAILURE;

		RECT rc = { x, y, 0, 0 };
		char text[256];
		memset(text,'\0',256);
		va_list args;

		va_start(args, str);
		vsprintf(text, str, args);
		va_end(args);

		m_pTextSprite->Begin( D3DXSPRITE_ALPHABLEND | D3DXSPRITE_SORT_TEXTURE );

		SetRect( &rc, x, y, 0, 0 );
		//pD3DFont->DrawText(NULL,text, -1, &rc, DT_SINGLELINE, D3DCOLOR_COLORVALUE(clr.fRed,clr.fGreen,clr.fBlue,clr.fA));
		pD3DFont->DrawText(m_pTextSprite,text, -1, &rc, DT_SINGLELINE, D3DCOLOR_COLORVALUE(color.fRed,color.fGreen,color.fBlue,color.fA));

		m_pTextSprite->End();

		return SMT_ERR_NONE;
	}
}