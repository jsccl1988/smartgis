#include "baog_baorthgrid.h"
#include <fstream>
#include <iomanip>
using namespace std;

namespace  Smt_BAOrthGrid
{
	SmtBAOrthGrid::SmtBAOrthGrid(void):m_rMainRegion(typeMain)
	{
		m_nX = 0;
		m_nY = 0;

		m_pNodes = NULL;
		m_pCells = NULL;
		m_pOrthogonality = NULL;
	}

	SmtBAOrthGrid::SmtBAOrthGrid(int nX,int nY):m_rMainRegion(typeMain)
	{
		m_nX = 0;
		m_nY = 0;
		m_pNodes = NULL;
		m_pCells = NULL;
		m_pOrthogonality = NULL;

		SetSize(nX, nY);
	}

	SmtBAOrthGrid::~SmtBAOrthGrid()
	{
		int nNum = m_rRegions.size();
		for(int i=0;i<nNum;i++)
		{	
			SmtBAOGRegion * pRegion = m_rRegions.at(i);
			if(pRegion) 
				delete pRegion;
		}
		m_rRegions.clear();

		Release();
	}

	void SmtBAOrthGrid::Release()
	{
		SMT_SAFE_DELETE(m_pNodes);
		SMT_SAFE_DELETE(m_pCells);
		SMT_SAFE_DELETE(m_pOrthogonality);

		m_nX = 0;
		m_nY = 0;
	}

	//////////////////////////////////////////////////////////////////////////
	void SmtBAOrthGrid::SetSize(int nX,int nY)
	{
		if(nX == m_nX && nY == m_nY) 
			return;
		//先释放原有空间
		Release();

		//重置参数
		m_nX = nX;
		m_nY = nY;

		//申请空间
		m_pNodes = new Matrix2D<dbfPoint>(m_nY,m_nX);
		m_pCells = new Matrix2D<GridCell>(m_nY-1,m_nX-1);
		m_pOrthogonality = new Matrix2D<float>(m_nY,m_nX);

		if ( !m_pCells || !m_pNodes) 
			return;

		GridCell cell;
		cell.IsSelected = false;
		cell.MaskerClr = RGB(255,177,0);
		for (int i = 0; i < m_nX - 1; i ++)
		{
			for (int j = 0; j < m_nY - 1; j++)
			{
				m_pCells->SetElement(cell,j,i);
			}
		}

		float orth = 0.;
		for (int i = 0; i < m_nX; i ++)
		{
			for (int j = 0; j < m_nY ; j++)
			{
				m_pOrthogonality->SetElement(orth,j,i);
			}
		}
	}

	//////////////////////////////////////////////////////////////////////////
	//重置网格大小
	void SmtBAOrthGrid::ReSize(int nX,int nY)
	{
		if(nX == m_nX && nY == m_nY) 
			return;

		SetSize(nX,nY);

		int nNum = m_rRegions.size();
		for(int i=0;i<nNum;i++)
		{	
			SmtBAOGRegion * pRegion = m_rRegions.at(i);
			SMT_SAFE_DELETE(pRegion);
		}

		m_rRegions.clear();

		m_rMainRegion.ReSizeBoudary(0,0,m_nX-1,0,0); 
		m_rMainRegion.ReSizeBoudary(1,0,m_nY-1,m_nX-1,1);
		m_rMainRegion.ReSizeBoudary(2,m_nX-1,0,m_nY-1,2);  
		m_rMainRegion.ReSizeBoudary(3,m_nY-1,0,0,3); 

		CreateOrthGrid();
	}

	void SmtBAOrthGrid::GetSize(int &nX,int &nY)
	{
		nX = m_nX;
		nY = m_nY;
	}

	void SmtBAOrthGrid::InitalCell(bool sel)
	{
		GridCell cell;
		for (int i = 0; i < m_nX - 1; i ++)
		{
			for (int j = 0; j < m_nY - 1; j++)
			{
				cell = m_pCells->GetElement(j,i);
				cell.IsSelected = sel;
				m_pCells->SetElement(cell,j,i);
			}
		}
	}

	//////////////////////////////////////////////////////////////////////////
	//设置网格边界数据
	void SmtBAOrthGrid::SetMainRegoinBoudary(vdbfPoints &bnd0,vdbfPoints &bnd1,vdbfPoints &bnd2,vdbfPoints &bnd3)
	{
		//添加主区边界
		m_rMainRegion.AppendBoudary(bnd0,0,m_nX-1,0,0);        //添加下边
		m_rMainRegion.AppendBoudary(bnd1,0,m_nY-1,m_nX-1,1);   //添加右边
		m_rMainRegion.AppendBoudary(bnd2,m_nX-1,0,m_nY-1,2);   //添加上边
		m_rMainRegion.AppendBoudary(bnd3,m_nY-1,0,0,3);        //添加左边

		m_rMainRegion.SetSlided(0,true);
		m_rMainRegion.SetSlided(1,true);
		m_rMainRegion.SetSlided(2,true);
		m_rMainRegion.SetSlided(3,true);

		//光滑边界曲线
		m_rMainRegion.SmoothBoundary();
	}

	void SmtBAOrthGrid::SetAppendMainRegoinBoudary(vdbfPoints &bnd0,vdbfPoints &bnd1,vdbfPoints &bnd2,vdbfPoints &bnd3)
	{
		//添加主区边界
		m_rMainRegion.AppendBoudary(bnd0,0,m_nX-1,0,0,false,false);        //添加下边
		m_rMainRegion.AppendBoudary(bnd1,0,m_nY-1,m_nX-1,1);   //添加右边
		m_rMainRegion.AppendBoudary(bnd2,m_nX-1,0,m_nY-1,2);   //添加上边
		m_rMainRegion.AppendBoudary(bnd3,m_nY-1,0,0,3);        //添加左边

		//光滑边界曲线
		m_rMainRegion.SmoothBoundary();
	}

	//////////////////////////////////////////////////////////////////////////
	//导入网格边界文件
	long SmtBAOrthGrid::LoadGridBndFromFile(const char * file)
	{
		locale loc = locale::global(locale(".936"));
		ifstream fin(file);
		locale::global(locale(loc));
		if (fin.bad())
		{
			return SMT_ERR_FAILURE;
		}

		char buf[256];
		fin.getline(buf,256,'\n');

		if(strcmp("gridbnd:",buf) != 0)
		{
			::MessageBox(NULL,"格式错误","读取文件 - error", MB_OK | MB_ICONERROR);
			fin.close();
			return SMT_ERR_FAILURE;
		}

		int nX,nY;
		int lData;

		fin.getline(buf,256,'\n');
		lData = sscanf(buf,"%d %d",&nX ,&nY);
		if (lData != 2)
		{
			fin.close();
			return SMT_ERR_FAILURE;
		}

		while (strcmp("begin",buf) != 0)
			fin.getline(buf,256,'\n');

		//read main region
		ReadMainRegion(fin);

		//read digged region	
		ReadDiggedRegion(fin);

		fin.close();	
		SetSize(nX,nY);

		return SMT_ERR_NONE;
	}

	//导出网格边界文件
	long SmtBAOrthGrid::SaveGridBndToFile(const char * file)
	{
		locale loc = locale::global(locale(".936"));
		ofstream fout(file);
		locale::global(locale(loc));

		if (fout.bad())
		{
			return SMT_ERR_FAILURE;
		}

		//fout.setf(ios_base::fixed,ios_base::floatfield);
		fout << "gridbnd:" << endl;
		fout << m_nX << " "<< m_nY << endl;

		fout << "begin"<<endl;

		//write main region	
		WriteMainRegion(fout);

		//write digged regions
		WriteDiggedRegion(fout);

		fout << "end"<<endl;	
		fout.close();

		return SMT_ERR_NONE;
	}

	void SmtBAOrthGrid::WriteMainRegion(std::ofstream &fout)
	{
		char szIOBuf[TEMP_BUFFER_SIZE];
		
		fout <<"main_begin"<<endl;
		int nBnds = m_rMainRegion.m_vBnds.size();
		for (int i = 0; i < nBnds; i ++ )
		{
			SmtBAOGBoudary * pBnd = m_rMainRegion.m_vBnds.at(i);
			int n = pBnd->ctrlPts.size();
			fout << n << endl;
			fout << pBnd->start << " " <<pBnd->end << " " <<pBnd->index << " " << pBnd->flag <<" " << pBnd->can_slide <<" "<< pBnd->can_sample<< endl;
			for (int i = 0; i < n; i ++)
			{
				dbfPoint P = pBnd->ctrlPts[i].P;
				sprintf(szIOBuf,"%lf,%lf",P.x,P.y);
				fout<<szIOBuf<<endl;
			}   
		} 
		fout <<"main_end"<<endl;
	}

	void SmtBAOrthGrid::ReadMainRegion(std::ifstream &fin)
	{
		dbfPoint pt;
		char buf[256];

		vCurvePoints ctrlPts;

		while (strcmp("main_begin",buf) != 0)fin.getline(buf,256,'\n');

		fin.getline(buf,256,'\n');
		while (strcmp("main_end",buf) != 0)
		{			
			int n;
			int start,end;
			int index;
			int flag;
			int slided;
			int sampled;
			vCurvePoints ctrlPts;
			sscanf(buf,"%d",&n);
			fin.getline(buf,256,'\n');
			sscanf(buf,"%d %d %d %d %d %d",&start,&end,&index,&flag,&slided,&sampled);
			ctrlPts.resize(n);
			for (int i = 0; i < n; i ++)
			{
				fin.getline(buf,256,'\n');
				sscanf(buf,"%lf,%lf",&pt.x ,&pt.y);
				ctrlPts[i] = pt;
			}

			m_rMainRegion.AppendBoudary(ctrlPts,start,end,index,flag,slided,sampled);

			fin.getline(buf,256,'\n');
		}
		//光滑边界曲线
		m_rMainRegion.SmoothBoundary();
	}

	void SmtBAOrthGrid::WriteDiggedRegion(std::ofstream &fout)
	{
		char szIOBuf[TEMP_BUFFER_SIZE];
		for (int i = 0 ; i < m_rRegions.size() ; i++)
		{
			SmtBAOGRegion * pRegion = m_rRegions[i];
			fout <<"digged_begin"<<endl;
			for (int j = 0; j < pRegion->m_vBnds.size(); j ++ )
			{
				SmtBAOGBoudary * pBnd = pRegion->m_vBnds.at(j);
				int n = pBnd->ctrlPts.size();
				fout << n << endl;
				fout << pBnd->start << " " <<pBnd->end << " " <<pBnd->index << " " << pBnd->flag <<" " <<  pBnd->can_slide <<" "<< pBnd->can_sample<< endl;
				for (int k = 0; k < n; k ++)
				{
					dbfPoint P = pBnd->ctrlPts[k].P;
					sprintf(szIOBuf,"%lf,%lf",P.x,P.y);
					fout<<szIOBuf<<endl;
				}   
			} 
			fout <<"digged_end"<<endl;
		}

	}

	void SmtBAOrthGrid::ReadDiggedRegion(std::ifstream &fin)
	{
		dbfPoint pt;
		char buf[256];
		while (strcmp("digged_begin",buf) != 0 && strcmp("end",buf) != 0)fin.getline(buf,256,'\n');
		while (strcmp("end",buf) != 0)
		{
			SmtBAOGRegion *pRegion = new SmtBAOGRegion(typeDigged);
			fin.getline(buf,256,'\n');
			while (strcmp("digged_end",buf) != 0)
			{				
				int n;
				int start,end;
				int index;
				int flag;
				int slided;
				int sampled;
				vCurvePoints ctrlPts;
				sscanf(buf,"%d",&n);
				fin.getline(buf,256,'\n');
				sscanf(buf,"%d %d %d %d %d %d",&start,&end,&index,&flag,&slided,&sampled);
				ctrlPts.resize(n);
				for (int i = 0; i < n; i ++)
				{
					fin.getline(buf,256,'\n');
					sscanf(buf,"%lf,%lf",&pt.x ,&pt.y);
					ctrlPts[i] = pt;
				}

				pRegion->AppendBoudary(ctrlPts,start,end,index,flag,slided,sampled);
				fin.getline(buf,256,'\n');
			}

			//光滑边界曲线
			pRegion->SmoothBoundary(15);
			m_rRegions.push_back(pRegion);

			while (strcmp("digged_begin",buf) != 0 && strcmp("end",buf) != 0)fin.getline(buf,256,'\n');
		}
	}
}