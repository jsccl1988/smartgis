#include "gl_3drenderdevice.h"

using namespace Smt_Core;

namespace Smt_3Drd
{
	//////////////////////////////////////////////////////////////////////////
	//print implement
	long SmtGLRenderDevice::DrawCube3D(Vector3 vCenter,float fWidth,SmtColor smtClr)
	{
		float width = fWidth/2.;
		Vector3 vTopLeftFront( vCenter.x - width, vCenter.y + width, vCenter.z + width);
		Vector3 vTopLeftBack(  vCenter.x - width, vCenter.y + width, vCenter.z - width);
		Vector3 vTopRightBack( vCenter.x + width, vCenter.y + width, vCenter.z - width);
		Vector3 vTopRightFront(vCenter.x + width, vCenter.y + width, vCenter.z + width);

		Vector3 vBottomLeftFront( vCenter.x - width, vCenter.y - width, vCenter.z + width);
		Vector3 vBottomLeftBack(  vCenter.x - width, vCenter.y - width, vCenter.z - width);
		Vector3 vBottomRightBack( vCenter.x + width, vCenter.y - width, vCenter.z - width);
		Vector3 vBottomRightFront(vCenter.x + width, vCenter.y - width, vCenter.z + width);

		glColor4f(smtClr.fRed,smtClr.fGreen,smtClr.fBlue,smtClr.fA);			     
		glBegin(GL_LINES);		
		////////// TOP LINES ////////// 
		// Store the top front line of the box
		glVertex3f(vTopLeftFront.x,vTopLeftFront.y,vTopLeftFront.z);
		glVertex3f(vTopRightFront.x,vTopRightFront.y,vTopRightFront.z);	

		// Store the top back line of the box
		glVertex3f(vTopLeftBack.x,vTopLeftBack.y,vTopLeftBack.z);
		glVertex3f(vTopRightBack.x,vTopRightBack.y,vTopRightBack.z);

		// Store the top left line of the box
		glVertex3f(vTopLeftFront.x,vTopLeftFront.y,vTopLeftFront.z);
		glVertex3f(vTopLeftBack.x,vTopLeftBack.y,vTopLeftBack.z);

		// Store the top right line of the box
		glVertex3f(vTopRightFront.x,vTopRightFront.y,vTopRightFront.z);
		glVertex3f(vTopRightBack.x,vTopRightBack.y,vTopRightBack.z);

		////////// BOTTOM LINES ////////// 
		// Store the bottom front line of the box
		glVertex3f(vBottomLeftFront.x,vBottomLeftFront.y,vBottomLeftFront.z);
		glVertex3f(vBottomRightFront.x,vBottomRightFront.y,vBottomRightFront.z);

		// Store the bottom back line of the box
		glVertex3f(vBottomLeftBack.x,vBottomLeftBack.y,vBottomLeftBack.z);
		glVertex3f(vBottomRightBack.x,vBottomRightBack.y,vBottomRightBack.z);

		// Store the bottom left line of the box
		glVertex3f(vBottomLeftFront.x,vBottomLeftFront.y,vBottomLeftFront.z);
		glVertex3f(vBottomLeftBack.x,vBottomLeftBack.y,vBottomLeftBack.z);

		// Store the bottom right line of the box
		glVertex3f(vBottomRightFront.x,vBottomRightFront.y,vBottomRightFront.z);
		glVertex3f(vBottomRightBack.x,vBottomRightBack.y,vBottomRightBack.z);

		////////// SIDE LINES ////////// 
		// Store the bottom front line of the box
		glVertex3f(vTopLeftFront.x,vTopLeftFront.y,vTopLeftFront.z);
		glVertex3f(vBottomLeftFront.x,vBottomLeftFront.y,vBottomLeftFront.z);

		// Store the back left line of the box
		glVertex3f(vTopLeftBack.x,vTopLeftBack.y,vTopLeftBack.z);
		glVertex3f(vBottomLeftBack.x,vBottomLeftBack.y,vBottomLeftBack.z);

		// Store the front right line of the box
		glVertex3f(vTopRightBack.x,vTopRightBack.y,vTopRightBack.z);
		glVertex3f(vBottomRightBack.x,vBottomRightBack.y,vBottomRightBack.z);

		// Store the front left line of the box
		glVertex3f(vTopRightFront.x,vTopRightFront.y,vTopRightFront.z);
		glVertex3f(vBottomRightFront.x,vBottomRightFront.y,vBottomRightFront.z);

		glEnd();

		return SMT_ERR_NONE;
	}
}
