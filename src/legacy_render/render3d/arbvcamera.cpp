#include "legacy_render/render3d/camera.h"
#include "legacy_render/render3d/3drenderdevice.h"

namespace render
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
		// �Ӿ۽���ָ���ӵ������  OA����
		Vector3 vDir(m_vEye - m_vTarget);

		// ������������뾶 
		//if (m_fRaduis == 0)
		m_fRaduis = vDir.length();
		 
		// ���䵥λ��      
		vDir.normalize();    
		// ��ǰ������Ϸ�����a����ˣ�����ͶӰ��ˮƽ���ҷ�������u      
		Vector3  u = m_vUp.cross(vDir);     
		// ���䵥λ��19      
		u.normalize();

		// ����������Ϸ�����ͶӰ���ϵ�ͶӰ���� ����ֱ���ϵķ�������v22      
		Vector3 v = vDir.cross(u);    
		// ���䵥λ��      
		v.normalize();    

		// ������ĻAB��ͶӰ���϶�Ӧ������ AB����27      
		Vector3 m = u*deltX + v*deltY;  
		// ����m�����ĳ���      
		double len = m.length();     
		// ����������      
		len /= 15.0;
		if (len>0.0)   
		{       
			// �Ƕ�AOB  ���ȱ�ʾ ����/�뾶        
			double x = len/m_fRaduis;   
			// ��AB������λ��39        
			m.normalize(); 
			// ���෴����ת���ӵ㵽C �Ӷ�ʹ�ð�������ƶ�һ�µķ���ת��ģ��42         
			x = -1*x;   
			// �����µ����λ�� C
			m_vEye = m_vTarget+(vDir*cos(x) + m*sin(x))*m_fRaduis; 
			// �����µ�������Ϸ���        
			m_vUp = v;
		}
	}
}