#ifndef _BAOG_BAORTHOGRID_H
#define _BAOG_BAORTHOGRID_H


#include "baog_region.h"
#include "baog_bas_struct.h"
#include "smt_matrix2d.h"

/*
作者：陈春亮
说明：Boundary adaptive orthogonal grid
为了提高网格的正交性，尽量使网格四个角点保持正交，因为本程序不对角点进行调整
*/
namespace Smt_BAOrthGrid
{
	class SMT_EXPORT_CLASS SmtBAOrthGrid
	{
	public:
		SmtBAOrthGrid(void);
		SmtBAOrthGrid(int nX,int nY);
		virtual ~SmtBAOrthGrid(void);

	protected:
		void				Release(void);

	public:
		//////////////////////////////////////////////////////////////////////////
		//网格尺寸
		void				SetSize(int nX,int nY);
		void				ReSize(int nX,int nY);
		void				GetSize(int &nX,int &nY);

		//////////////////////////////////////////////////////////////////////////
		//边界数据接口
		long				LoadGridBndFromFile(const char * file);//导入网格边界文件
		long				SaveGridBndToFile(const char * file);  //导出网格边界文件

	public:
		//设置边界
		void				SetMainRegoinBoudary(vdbfPoints &bnd0,vdbfPoints &bnd1,vdbfPoints &bnd2,vdbfPoints &bnd3);
		void				SetAppendMainRegoinBoudary(vdbfPoints &bnd0,vdbfPoints &bnd1,vdbfPoints &bnd2,vdbfPoints &bnd3);

		//创建网格
		long				CreateOrthGrid(void);

		//转换成grid
		long				CvtToGrid(SmtGrid &oSmtGrid);

		//////////////////////////////////////////////////////////////////////////
		//初始化网格单元状态
		void				InitalCell(bool sel);

		//设置网格单元
		void				SetGridCell(void);                

		//计算正交化程度
		void				CalGridOrthogonality(void);

		//////////////////////////////////////////////////////////////////////////
		//编辑网格边界
		bool				AddCtrlPt(dbfPoint pt,int a = 10);        //添加边界控制点
		void				ModCtrlPt(dbfPoint pt);                   //修改边界控制点

		//删除边界控制点
		bool				DelCtrlPt(dbfPoint pt,int a = 10);

		//////////////////////////////////////////////////////////////////////////
		//挖去区
		bool				DigRectRegion(int iCellStart,int jCellStart,int iCellEnd ,int jCellEnd);
		void				GetRegionBoudaryX(vCurvePoints &bnd,int iStart,int iEnd,int J);
		void				GetRegionBoudaryY(vCurvePoints &bnd,int jStart,int jEnd,int I);
		void				SetDiggedRegionInvalid(void);

		//////////////////////////////////////////////////////////////////////////
		//悬挂区
		void				GetBoudaryNode(int start,int end, int index, int flag,vdbfPoints &nodes);
		void				GetBoudaryNodeX(int start,int end, int index,vdbfPoints &nodes);
		void				GetBoudaryNodeY(int start,int end, int index,vdbfPoints &nodes);

	protected:
		/////////////////////////////////////////////////////////////////////////
		//创建网格
		void				InitialGrid(void);                      //初始化网格

		//////////////////////////////////////////////////////////////////////////
		//求解拉普拉斯方程                
		void				Orhogonal_SOR(void);                    //超松弛迭代法
		
	protected:
		/************************************************************************/
		/* 有源                                                                     */
		/************************************************************************/
		//////////////////////////////////////////////////////////////////////////
		//势流理论
		void				CalP0(Matrix2D<double> *P);
		void				CalQ0(Matrix2D<double> *Q);

		//////////////////////////////////////////////////////////////////////////
		//基于边界
		void				CalP1(Matrix2D<double> *P);
		void				CalQ1(Matrix2D<double> *Q);
		//基于边界
		void				CalP3(Matrix2D<double> *P);
		void				CalQ3(Matrix2D<double> *Q);

		//魏文礼
		void				CalP2(Matrix2D<double> *P);
		void				CalQ2(Matrix2D<double> *Q);

	protected:
		//////////////////////////////////////////////////////////////////////////
		//调整边界，使得边界保持正交
		void				SlideBoudary(void); 
		void				SlideRegionBoudary(void);
		void				SlideRegionBoudary(SmtBAOGRegion *pRegion);
		void				AjustRegionCornerNode(SmtBAOGRegion *pRegion);
		void				AjustRegionCornerNode(int i,int j);
		void				SlideBoudary(SmtBAOGBoudary * pBnd);	
		void				SlideBoudaryX(vCurvePoints &bnd,int iStart,int iEnd,int J);
		void				SlideBoudaryY(vCurvePoints &bnd,int jStart,int jEnd,int I);

		//////////////////////////////////////////////////////////////////////////
		//初始化网格
		void				InitialBoudary();                       //初始化边界
		void				InitialRegionBoudary();
		void				InitialRegionBoudary(SmtBAOGRegion * pRegion);
		void				SampleBoudary(SmtBAOGBoudary * pBnd);
		void				SampleBoudaryX(vCurvePoints &bnd,int iStart,int iEnd,int J);
		void				SampleBoudaryY(vCurvePoints &bnd,int jStart,int jEnd,int I);
		void				SetBoudaryX(vCurvePoints &bnd,int iStart,int iEnd,int J);
		void				SetBoudaryY(vCurvePoints &bnd,int jStart,int jEnd,int I);

		void				InitialInternal();                      //初始化内部网格节点

	protected:
		//////////////////////////////////////////////////////////////////////////
		//网格数据接口
		void				WriteMainRegion(std::ofstream &fout);
		void				ReadMainRegion(std::ifstream &fin);
		void				WriteDiggedRegion(std::ofstream &fout);
		void				ReadDiggedRegion(std::ifstream &fin);
	
	protected:
		void				AddOrth(int i, int j,float add);
		bool				IsOnDiggedRegion(int ii,int jj);
		bool				IsInDiggedRegion(int ii,int jj);

	protected:
		vRegionPtrs			m_rRegions;
		SmtBAOGRegion		m_rMainRegion;

	protected:
		Matrix2D<dbfPoint>	*m_pNodes;                //网格格点坐标
		Matrix2D<GridCell>	*m_pCells;                //网格单元
		Matrix2D<float>		*m_pOrthogonality;        //正交化程度   

		int					m_nX;					  //网线列数
		int					m_nY;	                  //网线行数
	};
}

#if !defined(Export_SmtBAOrthGrid)
#if   defined( _DEBUG)
#          pragma comment(lib,"SmtBAOrthGridD.lib")
#       else
#          pragma comment(lib,"SmtBAOrthGrid.lib")
#	    endif  
#endif


#endif// _BAOG_BAORTHOGRID_H
