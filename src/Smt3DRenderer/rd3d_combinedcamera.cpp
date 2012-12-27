#include "rd3d_base.h"

namespace Smt_3Drd
{
	
	SmtCombinedCamera::SmtCombinedCamera()
	{

		Vector3 zero = Vector3(0.,0.,0.);		
		Vector3 view = Vector3(0.0,1.0, 0.5);		
		Vector3 up   = Vector3(0.,0.,1.);		
    
		m_vEye	= zero;					
		m_vTarget	= view;				
		m_vUp	= up;	
	}

	SmtCombinedCamera::~SmtCombinedCamera(void)
	{
    
	}

	void SmtCombinedCamera::SetCamera(Vector3 &pos,Vector3 &view,Vector3 &up)
	{
		m_vEye = pos;
		m_vUp = up;
		m_vTarget = view;
	}

	//move camera
	void SmtCombinedCamera::ShiftCamera(float step)
	{
		Vector3 vCross,vDir(m_vTarget - m_vEye);
		vCross = vDir.CrossProduct(m_vUp);
		vCross.Normalize();

		m_vEye.x += vCross.x * step;
		m_vEye.z += vCross.z * step;
		
		m_vTarget.x += vCross.x * step;
		m_vTarget.z += vCross.z * step;
	}

	void SmtCombinedCamera::ForwardCamera(float step)
	{
		Vector3 vDir=m_vTarget-m_vEye;
		vDir.Normalize();
		
		m_vEye.x+=vDir.x*step;
		m_vEye.z+=vDir.z*step;
		m_vTarget.x+=vDir.x*step;
		m_vTarget.z+=vDir.z*step;
	}

	void SmtCombinedCamera::RiseCamera(float step)
	{
       m_vEye.y += m_vUp.y * step;
	   m_vTarget.y += m_vUp.y * step;
	}

	void SmtCombinedCamera::LeanCamera(float angle)
	{
		Vector3 vDir=m_vTarget-m_vEye;
		vDir.Normalize();
		m_vUp.Rotate(vDir,angle);			
	}

	void SmtCombinedCamera::MoveCamera(Vector3 &vec)
	{
	   m_vEye  += vec;
	   m_vTarget += vec;
	}

	void SmtCombinedCamera::MoveCameraToPos(Vector3 &pos)
	{
        Vector3 dV = pos - m_vEye;
		MoveCamera(dV);
	}

	void  SmtCombinedCamera::RaiseViewDirection(float angle)
	{
		Vector3 vCross,vDir(m_vTarget - m_vEye);
		vCross = vDir.CrossProduct(m_vUp);
		vCross.Normalize();
		vDir.Rotate(vCross,angle);
		m_vTarget = m_vEye + vDir;
	}

	void  SmtCombinedCamera::TurnViewDirection(float angle)
	{
		Vector3 vDir(m_vTarget - m_vEye);
		vDir.Rotate(m_vUp,angle);
		m_vTarget = m_vEye + vDir;
	}  

	//////////////////////////////////////////////////////////////////////////
	void SmtCombinedCamera::SetSphereCameraMove(long deltX,long deltY)
	{
		 // 从聚焦点指向视点的向量  OA向量
		Vector3 vDir(m_vEye - m_vTarget);
	
		// 计算球面相机半径      
		float radius = vDir.GetLength();     
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
			double x = len/radius;   
			// 将AB向量单位化39        
			m.Normalize(); 
			// 按相反方向转动视点到C 从而使得按与鼠标移动一致的方向转动模型42         
			x = -1*x;   
			// 计算新的相机位置 C
			m_vEye = m_vTarget+(vDir*cos(x) + m*sin(x))*radius; 
			// 计算新的相机向上方向        
			m_vUp = v;
		}
	}
}