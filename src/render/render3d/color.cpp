#include "render/render3d/base.h"

namespace render
{

	SmtColor::SmtColor(float red,float green,float blue,float a )
	{
		fRed   = red;
		fGreen = green;
		fBlue  = blue;
		fA     = a;
	}

	SmtColor::SmtColor(void)
	{
		fRed   = 0.;
		fGreen = 0.;
		fBlue  = 0.;
		fA     = 1.;
	}

}
