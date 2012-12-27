#include "gl_3drenderdevice.h"
#include "gl_indexbuffer.h"
#include "gl_vertexbuffer.h"
#include "smt_logmanager.h"
#include "gl_text.h"

using namespace Smt_Core;

namespace Smt_3Drd
{
	//////////////////////////////////////////////////////////////////////////
	// Rendering functions
	long SmtGLRenderDevice::BeginRender()
	{
		/*HDC hDC = ::GetDC(m_hWnd);

		wglMakeCurrent (hDC, m_hRC);

		::ReleaseDC(m_hWnd,hDC);*/

		return SMT_ERR_NONE;
	}

	long SmtGLRenderDevice::EndRender()
	{
		::glFlush();
		//wglMakeCurrent(NULL,NULL);
		return SMT_ERR_NONE;
	}

	long SmtGLRenderDevice::SwapBuffers()
	{
		HDC hDC = ::GetDC( m_hWnd );
		::SwapBuffers( hDC );
		::ReleaseDC(m_hWnd,hDC);

		return SMT_ERR_NONE;
	}

	long SmtGLRenderDevice::DrawPrimitives( PrimitiveType primitiveType, SmtVertexBuffer* pVB, DWORD baseVertex, DWORD primitiveCount )
	{
		if ( pVB==NULL )
			return SMT_ERR_INVALID_PARAM;

		// Convert primitive type
		GLenum PT;
		ulong count;
		if ( SMT_ERR_NONE != GetOpenGLPrimitiveType( primitiveType, primitiveCount, &PT, &count ) )
			return SMT_ERR_FAILURE;

		// Say that the VB will be the source for our draw primitive calls
		//--
		if ( SMT_ERR_NONE != pVB->PrepareForDrawing() )
			return SMT_ERR_FAILURE;

		// Draw primitives
		//--
		glDrawArrays( PT, baseVertex, count );

		if ( SMT_ERR_NONE != pVB->EndDrawing() )
			return SMT_ERR_FAILURE;

		return SMT_ERR_NONE;
	}

	long SmtGLRenderDevice::DrawIndexedPrimitives( PrimitiveType primitiveType, SmtVertexBuffer *pVB,SmtIndexBuffer *pIB,ulong baseIndex,ulong primitiveCount )
	{
		if ( pVB==NULL || pIB == NULL )
			return SMT_ERR_INVALID_PARAM;

		// Convert primitive type
		GLenum PT;
		ulong count;
		if ( SMT_ERR_NONE != GetOpenGLPrimitiveType( primitiveType, primitiveCount, &PT, &count ) )
			return SMT_ERR_FAILURE;

		// Say that the VB will be the source for our draw primitive calls
		//--
		if (SMT_ERR_NONE != pVB->PrepareForDrawing() ||
			SMT_ERR_NONE != pIB->PrepareForDrawing())
			return SMT_ERR_FAILURE;

		// Draw primitives
		//--
		glDrawElements( PT,count,GL_UNSIGNED_INT,pIB->GetIndexData());

		if ( SMT_ERR_NONE != pVB->EndDrawing() ||
			 SMT_ERR_NONE != pIB->EndDrawing() )
			return SMT_ERR_FAILURE;

		return SMT_ERR_NONE;
	}

	inline long SmtGLRenderDevice::GetOpenGLPrimitiveType(const PrimitiveType pt,const ulong nInitialPrimitiveCount,GLenum* GLPrimitiveType,ulong* nGLPrimitiveCount )
	{
		switch (pt)
		{
		case PT_POINTLIST:
			*GLPrimitiveType = GL_POINTS;
			*nGLPrimitiveCount = nInitialPrimitiveCount;
			return SMT_ERR_NONE;

		case PT_LINELIST:
			*GLPrimitiveType = GL_LINES;
			*nGLPrimitiveCount = 2*nInitialPrimitiveCount;
			return SMT_ERR_NONE;

		case PT_LINESTRIP:
			*GLPrimitiveType = GL_LINE_STRIP;
			*nGLPrimitiveCount = nInitialPrimitiveCount;
			return SMT_ERR_NONE;

		case PT_TRIANGLELIST:
			*GLPrimitiveType = GL_TRIANGLES;
			*nGLPrimitiveCount = 3*nInitialPrimitiveCount;
			return SMT_ERR_NONE;

		case PT_TRIANGLESTRIP:
			*GLPrimitiveType = GL_TRIANGLE_STRIP;
			*nGLPrimitiveCount = nInitialPrimitiveCount+2;
			return SMT_ERR_NONE;

		case PT_TRIANGLEFAN:
			*GLPrimitiveType = GL_TRIANGLE_FAN;
			*nGLPrimitiveCount = nInitialPrimitiveCount+2;
			return SMT_ERR_NONE;

		default:
			*GLPrimitiveType = GL_POINTS;
			*nGLPrimitiveCount = nInitialPrimitiveCount;
			return SMT_ERR_NONE;
		}

		return SMT_ERR_FAILURE;
	}

	long SmtGLRenderDevice::DrawText(uint unID,float x, float y,float z, const SmtColor& color,const char *str, ...) 
	{
		if(str == NULL ||
			unID > m_vTextPtrs.size())	
			return SMT_ERR_INVALID_PARAM;

		SmtGLText *pText = m_vTextPtrs.at(unID);
		if(NULL == pText)
			return SMT_ERR_INVALID_PARAM;

		char text[256];
		memset(text,'\0',256);
		va_list args;

		va_start(args, str);
		vsprintf(text, str, args);
		va_end(args);

		glColor4f (color.fRed, color.fGreen, color.fBlue,color.fA);

		glDisable(GL_LIGHTING);
		glDisable(GL_TEXTURE_2D);

		HDC hDC = ::GetDC( m_hWnd );
		pText->DrawText(hDC,x,y,z,text);
		::ReleaseDC(m_hWnd,hDC);

		glEnable(GL_LIGHTING);
		glEnable(GL_TEXTURE_2D);

		return SMT_ERR_NONE;
	}

	long SmtGLRenderDevice::DrawText(uint unID,float x, float y, const SmtColor& color,const char *str, ...) 
	{
		if(str == NULL ||
			unID > m_vTextPtrs.size())	
			return SMT_ERR_INVALID_PARAM;

		SmtGLText *pText = m_vTextPtrs.at(unID);
		if(NULL == pText)
			return SMT_ERR_INVALID_PARAM;

		char text[256];
		memset(text,'\0',256);
		va_list args;

		va_start(args, str);
		vsprintf(text, str, args);
		va_end(args);

		glColor4f (color.fRed, color.fGreen, color.fBlue,color.fA);

		glDisable(GL_LIGHTING);
		glDisable(GL_TEXTURE_2D);

		GLint viewport[4];
		glGetIntegerv(GL_VIEWPORT, viewport);

		glLoadIdentity();
		glMatrixMode(GL_PROJECTION);
		glPushMatrix();
		glLoadIdentity();
		gluOrtho2D(0, viewport[2], viewport[3], 0);
		glMatrixMode(GL_MODELVIEW);

		HDC hDC = ::GetDC( m_hWnd );
		pText->DrawText(hDC,x,y,text);
		::ReleaseDC(m_hWnd,hDC);

		glMatrixMode(GL_PROJECTION);
		glPopMatrix();
		glMatrixMode(GL_MODELVIEW);

		glEnable(GL_LIGHTING);
		glEnable(GL_TEXTURE_2D);

		return SMT_ERR_NONE;
	}
}