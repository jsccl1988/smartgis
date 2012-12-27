#include "md3d_pointcloud.h "

namespace Smt_3DModel
{
	Smt3DPointCloud::Smt3DPointCloud(void):m_bReadOK(false)
		,m_bShowOctNodeBox(true)
		,m_pVertexBuffer(NULL)
	{

	}

	Smt3DPointCloud::~Smt3DPointCloud()
	{
		Destroy();
	}

	long Smt3DPointCloud::Init(Vector3& vPos,SmtMaterial&matMaterial)
	{
		return Smt3DObject::Init(vPos,matMaterial);
	}

	long Smt3DPointCloud::Create(LP3DRENDERDEVICE p3DRenderDevice)
	{
		if (NULL == p3DRenderDevice)
		{
			return SMT_ERR_INVALID_PARAM;
		}

		if (!m_bReadOK || m_vtxList.nCount < 1 || m_vtxList.pVertexs == NULL)
		{
			return SMT_ERR_FAILURE;
		}

		/*m_pVertexBuffer = p3DRenderDevice->CreateVertexBuffer(
			m_vtxList.nCount,
			VF_XYZ|VF_DIFFUSE, 
			false );

		m_pVertexBuffer->Lock();

		for (int i = 0 ; i < m_vtxList.nCount; i ++ )
		{
			Vector3 vPos = m_vtxList.pVertexs[i].ver;
			SmtColor clr = m_vtxList.pVertexs[i].clr;

			m_pVertexBuffer->Vertex( vPos.x,vPos.y,vPos.z);
			m_pVertexBuffer->Diffuse( clr.fRed,clr.fGreen,clr.fBlue,clr.fA);

			m_aAbb.Merge(vPos);
		}

		m_pVertexBuffer->Unlock();*/
		 
		for (int i = 0 ; i < m_vtxList.nCount; i ++ )
		{
			Vector3 vPos = m_vtxList.pVertexs[i].ver;
			m_aAbb.Merge(vPos);
		}

		m_aAbb.vcCenter = (m_aAbb.vcMax + m_aAbb.vcMin)/2.;

		m_vtxOctTree.CreateOctTree(m_vtxList,p3DRenderDevice);
		
		//m_vtxOctTree.SaveToFile("oct.txt");

		return SMT_ERR_NONE;
	}

	long Smt3DPointCloud::Update(LP3DRENDERDEVICE p3DRenderDevice,float fElapsed)
	{
		return SMT_ERR_NONE;
	}

	long Smt3DPointCloud::Render(LP3DRENDERDEVICE p3DRenderDevice)
	{
		if (NULL == p3DRenderDevice)
		{
			return SMT_ERR_INVALID_PARAM;
		}

		p3DRenderDevice->SetMaterial(&m_matMaterial);

		p3DRenderDevice->MatrixPush();

		p3DRenderDevice->MatrixMultiply(m_mtxModel);

		/*if (m_pVertexBuffer)
		{
			p3DRenderDevice->DrawPrimitives(
				PT_POINTLIST,
				m_pVertexBuffer,
				0,
				m_pVertexBuffer->GetVertexCount());
		}*/

		m_vtxOctTree.RenderTree(p3DRenderDevice,m_bShowOctNodeBox);
		
		p3DRenderDevice->MatrixPop();

		return SMT_ERR_NONE;
	}

	long Smt3DPointCloud::Destroy()
	{
		SMT_SAFE_DELETE( m_pVertexBuffer);
		return SMT_ERR_NONE;
	}

	bool Smt3DPointCloud::Read3DPointCloud(const char* szFilePath)
	{
		ifstream infile;

		locale loc = locale::global(locale(".936"));
		infile.open(szFilePath,ios::in);
		locale::global(std::locale(loc));

		if (!infile.is_open())
		{
			return false;
		}

		char  szBuf[255];
	
		vSmtVertex3Ds	vVtxs;

		while(!infile.eof())
		{
			infile.getline(szBuf,255,'\n');
			SmtVertex3D pcVer;
			Vector3		ver;
			int			r,g,b;
			if (sscanf(szBuf,"%f,%f,%f,%d,%d,%d",&ver.x,&ver.z,&ver.y,&r,&g,&b) != 6)
			{
				continue;
			}

			pcVer.ver = ver*10;
			pcVer.clr.fRed = r/255.;
			pcVer.clr.fGreen = g/255.;
			pcVer.clr.fBlue = b/255.;

			vVtxs.push_back(pcVer);
		}

		infile.close();

		m_vtxList = vVtxs;

		m_bReadOK = true;

		return true;
	}
}