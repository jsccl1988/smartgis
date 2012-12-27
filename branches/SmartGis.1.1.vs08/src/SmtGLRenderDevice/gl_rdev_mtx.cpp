#include "gl_3drenderdevice.h"
#include "smt_logmanager.h"

using namespace Smt_Core;
namespace Smt_3Drd
{
	//////////////////////////////////////////////////////////////////////////
	// Tranformation functions
	long SmtGLRenderDevice::MatrixModeSet(MatrixMode mode)
	{
		m_matrixMode = mode;

		if ( m_matrixMode==MM_PROJECTION ) 
			glMatrixMode( GL_PROJECTION );
		else
			glMatrixMode( GL_MODELVIEW );

		return SMT_ERR_NONE;
	}

	MatrixMode SmtGLRenderDevice::MatrixModeGet() const
	{
		return m_matrixMode;
	}

	long SmtGLRenderDevice::MatrixLoadIdentity()
	{
		glLoadIdentity();
		return SMT_ERR_NONE;
	}

	long SmtGLRenderDevice::MatrixLoad( const Matrix& mtx )
	{
		glLoadMatrixf((float*)(&mtx));
		return SMT_ERR_NONE;
	}


	long SmtGLRenderDevice::MatrixPush()
	{
		glPushMatrix();
		return SMT_ERR_NONE;
	}

	long SmtGLRenderDevice::MatrixPop()
	{
		glPopMatrix();
		return SMT_ERR_NONE;
	}

	long SmtGLRenderDevice::MatrixScale( float x, float y, float z )
	{
		glScalef( x, y, z );
		return SMT_ERR_NONE;
	}

	long SmtGLRenderDevice::MatrixTranslation( float x, float y, float z )
	{
		glTranslatef( x, y, z );
		return SMT_ERR_NONE;
	}

	long SmtGLRenderDevice::MatrixRotation( float angle, float x, float y, float z )
	{
		glRotatef( angle, x, y, z );

		return SMT_ERR_NONE;
	}

	long SmtGLRenderDevice::MatrixMultiply(const Matrix& mtx)
	{
		glMultMatrixf( (float*)(&mtx) );

		return SMT_ERR_NONE;
	}

	Matrix SmtGLRenderDevice::MatrixGet()
	{
		Matrix mtxTmp;

		if ( m_matrixMode==MM_PROJECTION ) 
			glGetFloatv(GL_PROJECTION_MATRIX,(float *)&mtxTmp);
		else
			glGetFloatv(GL_MODELVIEW_MATRIX	,(float *)&mtxTmp);

		return mtxTmp;
	}
}