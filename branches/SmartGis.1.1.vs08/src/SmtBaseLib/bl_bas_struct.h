#ifndef _BL_BAS_STRUCT_H
#define _BL_BAS_STRUCT_H

//////////////////////////////////////////////////////////////////////////
namespace Smt_Base
{
	enum RenderBaseApi
	{
		RD_GDI,
		RD_GDIPLUS
	};

	struct  Viewport
	{
		float m_fVOX;
		float m_fVOY;
		float m_fVHeight;
		float m_fVWidth;

		Viewport():m_fVOX(0)
			,m_fVOY(0)
			,m_fVHeight(0)
			,m_fVWidth(0)
		{

		}
	};

	struct Windowport 
	{
		float m_fWOX;
		float m_fWOY;
		float m_fWHeight;
		float m_fWWidth;

		Windowport():m_fWOX(0)
			,m_fWOY(0)
			,m_fWHeight(0)
			,m_fWWidth(0)
		{

		}
	};

	struct Smt2DRenderPra 
	{
		bool		bShowMBR;
		bool		bShowPoint;
		long		lPointRaduis;

		Smt2DRenderPra():bShowMBR(true)
			,bShowPoint(true)
			,lPointRaduis(2)
		{
			;
		}
	};
}

#endif //_BL_BAS_STRUCT_H