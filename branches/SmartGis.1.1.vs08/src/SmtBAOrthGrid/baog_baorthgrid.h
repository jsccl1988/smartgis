#ifndef _BAOG_BAORTHOGRID_H
#define _BAOG_BAORTHOGRID_H


#include "baog_region.h"
#include "baog_bas_struct.h"
#include "smt_matrix2d.h"

/*
���ߣ��´���
˵����Boundary adaptive orthogonal grid
Ϊ���������������ԣ�����ʹ�����ĸ��ǵ㱣����������Ϊ�����򲻶Խǵ���е���
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
		//����ߴ�
		void				SetSize(int nX,int nY);
		void				ReSize(int nX,int nY);
		void				GetSize(int &nX,int &nY);

		//////////////////////////////////////////////////////////////////////////
		//�߽����ݽӿ�
		long				LoadGridBndFromFile(const char * file);//��������߽��ļ�
		long				SaveGridBndToFile(const char * file);  //��������߽��ļ�

	public:
		//���ñ߽�
		void				SetMainRegoinBoudary(vdbfPoints &bnd0,vdbfPoints &bnd1,vdbfPoints &bnd2,vdbfPoints &bnd3);
		void				SetAppendMainRegoinBoudary(vdbfPoints &bnd0,vdbfPoints &bnd1,vdbfPoints &bnd2,vdbfPoints &bnd3);

		//��������
		long				CreateOrthGrid(void);

		//ת����grid
		long				CvtToGrid(SmtGrid &oSmtGrid);

		//////////////////////////////////////////////////////////////////////////
		//��ʼ������Ԫ״̬
		void				InitalCell(bool sel);

		//��������Ԫ
		void				SetGridCell(void);                

		//�����������̶�
		void				CalGridOrthogonality(void);

		//////////////////////////////////////////////////////////////////////////
		//�༭����߽�
		bool				AddCtrlPt(dbfPoint pt,int a = 10);        //��ӱ߽���Ƶ�
		void				ModCtrlPt(dbfPoint pt);                   //�޸ı߽���Ƶ�

		//ɾ���߽���Ƶ�
		bool				DelCtrlPt(dbfPoint pt,int a = 10);

		//////////////////////////////////////////////////////////////////////////
		//��ȥ��
		bool				DigRectRegion(int iCellStart,int jCellStart,int iCellEnd ,int jCellEnd);
		void				GetRegionBoudaryX(vCurvePoints &bnd,int iStart,int iEnd,int J);
		void				GetRegionBoudaryY(vCurvePoints &bnd,int jStart,int jEnd,int I);
		void				SetDiggedRegionInvalid(void);

		//////////////////////////////////////////////////////////////////////////
		//������
		void				GetBoudaryNode(int start,int end, int index, int flag,vdbfPoints &nodes);
		void				GetBoudaryNodeX(int start,int end, int index,vdbfPoints &nodes);
		void				GetBoudaryNodeY(int start,int end, int index,vdbfPoints &nodes);

	protected:
		/////////////////////////////////////////////////////////////////////////
		//��������
		void				InitialGrid(void);                      //��ʼ������

		//////////////////////////////////////////////////////////////////////////
		//���������˹����                
		void				Orhogonal_SOR(void);                    //���ɳڵ�����
		
	protected:
		/************************************************************************/
		/* ��Դ                                                                     */
		/************************************************************************/
		//////////////////////////////////////////////////////////////////////////
		//��������
		void				CalP0(Matrix2D<double> *P);
		void				CalQ0(Matrix2D<double> *Q);

		//////////////////////////////////////////////////////////////////////////
		//���ڱ߽�
		void				CalP1(Matrix2D<double> *P);
		void				CalQ1(Matrix2D<double> *Q);
		//���ڱ߽�
		void				CalP3(Matrix2D<double> *P);
		void				CalQ3(Matrix2D<double> *Q);

		//κ����
		void				CalP2(Matrix2D<double> *P);
		void				CalQ2(Matrix2D<double> *Q);

	protected:
		//////////////////////////////////////////////////////////////////////////
		//�����߽磬ʹ�ñ߽籣������
		void				SlideBoudary(void); 
		void				SlideRegionBoudary(void);
		void				SlideRegionBoudary(SmtBAOGRegion *pRegion);
		void				AjustRegionCornerNode(SmtBAOGRegion *pRegion);
		void				AjustRegionCornerNode(int i,int j);
		void				SlideBoudary(SmtBAOGBoudary * pBnd);	
		void				SlideBoudaryX(vCurvePoints &bnd,int iStart,int iEnd,int J);
		void				SlideBoudaryY(vCurvePoints &bnd,int jStart,int jEnd,int I);

		//////////////////////////////////////////////////////////////////////////
		//��ʼ������
		void				InitialBoudary();                       //��ʼ���߽�
		void				InitialRegionBoudary();
		void				InitialRegionBoudary(SmtBAOGRegion * pRegion);
		void				SampleBoudary(SmtBAOGBoudary * pBnd);
		void				SampleBoudaryX(vCurvePoints &bnd,int iStart,int iEnd,int J);
		void				SampleBoudaryY(vCurvePoints &bnd,int jStart,int jEnd,int I);
		void				SetBoudaryX(vCurvePoints &bnd,int iStart,int iEnd,int J);
		void				SetBoudaryY(vCurvePoints &bnd,int jStart,int jEnd,int I);

		void				InitialInternal();                      //��ʼ���ڲ�����ڵ�

	protected:
		//////////////////////////////////////////////////////////////////////////
		//�������ݽӿ�
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
		Matrix2D<dbfPoint>	*m_pNodes;                //����������
		Matrix2D<GridCell>	*m_pCells;                //����Ԫ
		Matrix2D<float>		*m_pOrthogonality;        //�������̶�   

		int					m_nX;					  //��������
		int					m_nY;	                  //��������
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
