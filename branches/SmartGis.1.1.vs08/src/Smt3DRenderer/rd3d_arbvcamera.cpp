#include "rd3d_camera.h"
#include "rd3d_3drenderdevice.h"

namespace Smt_3Drd
{
	SmtArbvCamera::SmtArbvCamera(LP3DRENDERDEVICE	p3DRenderDevice,Viewport3D &viewport):SmtPerspCamera(p3DRenderDevice,viewport)
		,m_fRaduis(0)
	{
		;
	}

	SmtArbvCamera::~SmtArbvCamera(void)
	{	
		;
	}

	void SmtArbvCamera::SetArbitMove(long deltX,long deltY)
	{
		// 从聚焦点指向视点的向量  OA向量
		Vector3 vDir(m_vEye - m_vTarget);

		// 计算球面相机半径 
		//if (m_fRaduis == 0)
		m_fRaduis = vDir.GetLength();
		 
		// 将其单位化      
		vDir.Normalize();    
		// 当前相机向上方向与a做叉乘，计算投影面水平向右方向向量u      
		Vector3  u = m_vUp.CrossProduct(vDir);     
		// 将其单位化19      
		u.Normalize();

		// 计算相机向上方向在投影面上的投影向量 即垂直向上的方向向量v22      
		Vector3 v = vDir.CrossProduct(u);    
		// 将其单位化      
		v.Normalize();    

		// 计算屏幕AB在投影面上对应的向量 AB向量27      
		Vector3 m = u*deltX + v*deltY;  
		// 计算m向量的长度      
		double len = m.GetLength();     
		// 降低灵敏度      
		len /= 15.0;
		if (len>0.0)   
		{       
			// 角度AOB  弧度表示 弧长/半径        
			double x = len/m_fRaduis;   
			// 将AB向量单位化39        
			m.Normalize(); 
			// 按相反方向转动视点到C 从而使得按与鼠标移动一致的方向转动模型42         
			x = -1*x;   
			// 计算新的相机位置 C
			m_vEye = m_vTarget+(vDir*cos(x) + m*sin(x))*m_fRaduis; 
			// 计算新的相机向上方向        
			m_vUp = v;
		}
	}
}