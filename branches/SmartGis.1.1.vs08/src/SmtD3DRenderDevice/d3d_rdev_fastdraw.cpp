#include "d3d_3drenderdevice.h"

using namespace Smt_Core;

namespace Smt_3Drd
{
	//////////////////////////////////////////////////////////////////////////
	//print implement
	long SmtD3DRenderDevice::DrawCube3D(Vector3 vCenter,float fWidth,SmtColor smtClr)
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

		
		return SMT_ERR_NONE;
	}
}
