#include "gl_3drenderdevice.h"
#include "smt_logmanager.h"

using namespace Smt_Core;

namespace Smt_3Drd
{
	//////////////////////////////////////////////////////////////////////////
	// Other functions
	long SmtGLRenderDevice::SetClearColor(const SmtColor &clr)
	{
		glClearColor( clr.fRed,clr.fGreen,clr.fBlue,clr.fA);

		return SMT_ERR_NONE;
	}

	long  SmtGLRenderDevice::SetDepthClearValue( float z )
	{
		glClearDepth( z );

		return SMT_ERR_NONE;
	}

	long SmtGLRenderDevice::SetStencilClearValue( ulong s )
	{
		glClearStencil( s );

		return SMT_ERR_NONE;
	}

	long SmtGLRenderDevice::Clear( DWORD flags )
	{
		GLbitfield glFlags = 0;

		if ( flags & CLR_COLOR )
			glFlags |= GL_COLOR_BUFFER_BIT;

		if ( flags & CLR_ZBUFFER )
			glFlags |= GL_DEPTH_BUFFER_BIT;

		if ( flags & CLR_STENCIL )
			glFlags |= GL_STENCIL_BUFFER_BIT;

		glClear( glFlags );

		return SMT_ERR_NONE;
	}

	long SmtGLRenderDevice::SetViewport(Viewport3D &viewport)
	{
		m_viewPort = viewport;

		if(m_viewPort.ulHeight==0 || m_viewPort.ulWidth == 0)
			return SMT_ERR_FAILURE;

		glViewport( viewport.ulX, viewport.ulY ,viewport.ulWidth ,viewport.ulHeight);
		if (GL_NO_ERROR != glGetError())
			return SMT_ERR_FAILURE;

		glScissor(viewport.ulX, viewport.ulY ,viewport.ulWidth ,viewport.ulHeight);
		if (GL_NO_ERROR != glGetError())
			return SMT_ERR_FAILURE;

		return SMT_ERR_NONE;
	}

	long  SmtGLRenderDevice::SetOrtho(float left, float right, float bottom, float top, float zNear, float zFar)
	{
		::glOrtho(left, right, bottom, top, zNear, zFar);

		if (GL_NO_ERROR != glGetError())
			return SMT_ERR_FAILURE;

		return SMT_ERR_NONE;
	}

	long  SmtGLRenderDevice::SetPerspective( float fovy, float aspect, float zNear, float zFar)
	{
		::gluPerspective(fovy,aspect,zNear,zFar);

		if (GL_NO_ERROR != glGetError())
			return SMT_ERR_FAILURE;

		return SMT_ERR_NONE;
	}

	long SmtGLRenderDevice::SetViewLookAt(Vector3 &vPos,Vector3 &vView,Vector3 &vUp)
	{
		gluLookAt(vPos.x,vPos.y,vPos.z,vView.x,vView.y,vView.z,vUp.x,vUp.y,vUp.z);
		if (GL_NO_ERROR != glGetError())
			return SMT_ERR_FAILURE;

		return SMT_ERR_NONE;
	}

	//calculator 3D pos by 2D pos
	long SmtGLRenderDevice::Transform2DTo3D(Vector3 &vOrg,Vector3 &vTar,const lPoint &point)
	{
		int nCursorX = point.x, nCursorY = point.y;
		GLfloat fWinX = 0,fWinY = 0,fWinZ = 0;
		GLdouble _x = 0., _y = 0., _z = 0.;
		GLdouble modelview[16];
		GLdouble projection[16];
		GLint viewport[4];

		glPushMatrix();
		glGetIntegerv(GL_VIEWPORT, viewport);			// viewport
		glGetDoublev(GL_MODELVIEW_MATRIX, modelview);	// view matrix
		glGetDoublev(GL_PROJECTION_MATRIX, projection);	// projection matrix
		glPopMatrix();

		fWinX = (double)nCursorX;
		fWinY = (double)viewport[3] - (double)nCursorY - 1;

		glReadPixels(nCursorX,(int)fWinY,1,1,GL_DEPTH_COMPONENT,GL_FLOAT,&fWinZ);

		if (fWinZ >= 0 && fWinZ < 1.0)
		{
			// 获取近裁剪面上的交点
			gluUnProject( fWinX, fWinY,0.1, modelview, projection,viewport,&_x,&_y,&_z); 
			vOrg.Set(_x,_y,_z);

			// 获取远裁剪面上的交点
			gluUnProject( fWinX, fWinY,fWinZ, modelview, projection,viewport,&_x,&_y,&_z); 
			vTar.Set(_x,_y,_z);
		}
		
		return SMT_ERR_NONE;
	}

	//3d to 2d
	long SmtGLRenderDevice::Transform3DTo2D(const Vector3 &ver3D,lPoint &point)
	{
		GLdouble _x = 0., _y = 0., _z = 0.;
		GLdouble modelview[16];
		GLdouble projection[16];
		GLint viewport[4];

		glGetIntegerv(GL_VIEWPORT, viewport);			// viewport
		glGetDoublev(GL_MODELVIEW_MATRIX, modelview);	// view matrix
		glGetDoublev(GL_PROJECTION_MATRIX, projection);	// projection matrix

		gluProject( ver3D.x, ver3D.y, ver3D.z, modelview, projection,viewport,&_x,&_y,&_z);

		point.x = _x;
		point.y = _y;

		return SMT_ERR_NONE;
	}

	//////////////////////////////////////////////////////////////////////////
	void NormalizePlane(float frustum[6][4], int side)
	{
		// Here we calculate the magnitude of the normal to the plane (point A B C)
		// Remember that (A, B, C) is that same thing as the normal's (X, Y, Z).
		// To calculate magnitude you use the equation:  magnitude = sqrt( x^2 + y^2 + z^2)
		float magnitude = (float)sqrt( frustum[side][P_A] * frustum[side][P_A] + 
			frustum[side][P_B] * frustum[side][P_B] + 
			frustum[side][P_C] * frustum[side][P_C] );

		// Then we divide the plane's values by it's magnitude.
		// This makes it easier to work with.
		frustum[side][P_A] /= magnitude;
		frustum[side][P_B] /= magnitude;
		frustum[side][P_C] /= magnitude;
		frustum[side][P_D] /= magnitude; 
	}

	//get frustum
	long SmtGLRenderDevice::GetFrustum(SmtFrustum &smtFrustum)
	{
		float   frustum[6][4];

		float   proj[16];								// This will hold our projection matrix
		float   modl[16];								// This will hold our modelview matrix
		float   clip[16];								// This will hold the clipping planes


		glGetFloatv( GL_PROJECTION_MATRIX, proj );
		glGetFloatv( GL_MODELVIEW_MATRIX, modl );

		// Now that we have our modelview and projection matrix, if we combine these 2 matrices,
		// it will give us our clipping planes.  To combine 2 matrices, we multiply them.

		clip[ 0] = modl[ 0] * proj[ 0] + modl[ 1] * proj[ 4] + modl[ 2] * proj[ 8] + modl[ 3] * proj[12];
		clip[ 1] = modl[ 0] * proj[ 1] + modl[ 1] * proj[ 5] + modl[ 2] * proj[ 9] + modl[ 3] * proj[13];
		clip[ 2] = modl[ 0] * proj[ 2] + modl[ 1] * proj[ 6] + modl[ 2] * proj[10] + modl[ 3] * proj[14];
		clip[ 3] = modl[ 0] * proj[ 3] + modl[ 1] * proj[ 7] + modl[ 2] * proj[11] + modl[ 3] * proj[15];

		clip[ 4] = modl[ 4] * proj[ 0] + modl[ 5] * proj[ 4] + modl[ 6] * proj[ 8] + modl[ 7] * proj[12];
		clip[ 5] = modl[ 4] * proj[ 1] + modl[ 5] * proj[ 5] + modl[ 6] * proj[ 9] + modl[ 7] * proj[13];
		clip[ 6] = modl[ 4] * proj[ 2] + modl[ 5] * proj[ 6] + modl[ 6] * proj[10] + modl[ 7] * proj[14];
		clip[ 7] = modl[ 4] * proj[ 3] + modl[ 5] * proj[ 7] + modl[ 6] * proj[11] + modl[ 7] * proj[15];

		clip[ 8] = modl[ 8] * proj[ 0] + modl[ 9] * proj[ 4] + modl[10] * proj[ 8] + modl[11] * proj[12];
		clip[ 9] = modl[ 8] * proj[ 1] + modl[ 9] * proj[ 5] + modl[10] * proj[ 9] + modl[11] * proj[13];
		clip[10] = modl[ 8] * proj[ 2] + modl[ 9] * proj[ 6] + modl[10] * proj[10] + modl[11] * proj[14];
		clip[11] = modl[ 8] * proj[ 3] + modl[ 9] * proj[ 7] + modl[10] * proj[11] + modl[11] * proj[15];

		clip[12] = modl[12] * proj[ 0] + modl[13] * proj[ 4] + modl[14] * proj[ 8] + modl[15] * proj[12];
		clip[13] = modl[12] * proj[ 1] + modl[13] * proj[ 5] + modl[14] * proj[ 9] + modl[15] * proj[13];
		clip[14] = modl[12] * proj[ 2] + modl[13] * proj[ 6] + modl[14] * proj[10] + modl[15] * proj[14];
		clip[15] = modl[12] * proj[ 3] + modl[13] * proj[ 7] + modl[14] * proj[11] + modl[15] * proj[15];

		// Now we actually want to get the sides of the frustum.  To do this we take
		// the clipping planes we received above and extract the sides from them.

		// This will extract the RIGHT side of the frustum
		frustum[FS_RIGHT][P_A] = clip[ 3] - clip[ 0];
		frustum[FS_RIGHT][P_B] = clip[ 7] - clip[ 4];
		frustum[FS_RIGHT][P_C] = clip[11] - clip[ 8];
		frustum[FS_RIGHT][P_D] = clip[15] - clip[12];

		// Now that we have a normal (A,B,C) and a distance (D) to the plane,
		// we want to normalize that normal and distance.

		// Normalize the RIGHT side
		NormalizePlane(frustum, FS_RIGHT);

		// This will extract the LEFT side of the frustum
		frustum[FS_LEFT][P_A] = clip[ 3] + clip[ 0];
		frustum[FS_LEFT][P_B] = clip[ 7] + clip[ 4];
		frustum[FS_LEFT][P_C] = clip[11] + clip[ 8];
		frustum[FS_LEFT][P_D] = clip[15] + clip[12];

		// Normalize the LEFT side
		NormalizePlane(frustum, FS_LEFT);

		// This will extract the BOTTOM side of the frustum
		frustum[FS_BOTTOM][P_A] = clip[ 3] + clip[ 1];
		frustum[FS_BOTTOM][P_B] = clip[ 7] + clip[ 5];
		frustum[FS_BOTTOM][P_C] = clip[11] + clip[ 9];
		frustum[FS_BOTTOM][P_D] = clip[15] + clip[13];

		// Normalize the BOTTOM side
		NormalizePlane(frustum, FS_BOTTOM);

		// This will extract the TOP side of the frustum
		frustum[FS_TOP][P_A] = clip[ 3] - clip[ 1];
		frustum[FS_TOP][P_B] = clip[ 7] - clip[ 5];
		frustum[FS_TOP][P_C] = clip[11] - clip[ 9];
		frustum[FS_TOP][P_D] = clip[15] - clip[13];

		// Normalize the TOP side
		NormalizePlane(frustum, FS_TOP);

		// This will extract the BACK side of the frustum
		frustum[FS_BACK][P_A] = clip[ 3] - clip[ 2];
		frustum[FS_BACK][P_B] = clip[ 7] - clip[ 6];
		frustum[FS_BACK][P_C] = clip[11] - clip[10];
		frustum[FS_BACK][P_D] = clip[15] - clip[14];

		// Normalize the BACK side
		NormalizePlane(frustum, FS_BACK);

		// This will extract the FRONT side of the frustum
		frustum[FS_FRONT][P_A] = clip[ 3] + clip[ 2];
		frustum[FS_FRONT][P_B] = clip[ 7] + clip[ 6];
		frustum[FS_FRONT][P_C] = clip[11] + clip[10];
		frustum[FS_FRONT][P_D] = clip[15] + clip[14];

		// Normalize the FRONT side
		NormalizePlane(frustum, FS_FRONT);

		smtFrustum.SetFrustum(frustum);

		return SMT_ERR_NONE;
	}
}