/*
File:   smt_matrix2d.h

Desc:   二维动态数组模板类，nRow行，nCol列

Version:Version 1.0

Writter: 陈春亮

Date:   2011.01.17
*/
#ifndef _SMT_MATRIX_2D_H
#define _SMT_MATRIX_2D_H

#include <assert.h>

#define		NULLPTR		(T*)(0)	

template<class T> 
class Matrix2D
{
public:
	Matrix2D(void);
	Matrix2D(int nRow, int nCol);
	virtual ~Matrix2D(void);

public:
	//////////////////////////////////////////////////////////////////////////
	void				Resize(int nRow, int nCol);

	inline int			GetRowCount()  const { return m_nRow;}
	inline int			GetColCount()  const { return m_nCol;}

	inline void			CalcMat2DIndex(int &iRow,int &jCol,int index)  const ;
	inline int			CalcMat2DIndex(int iRow,int jCol)  const ;

	//////////////////////////////////////////////////////////////////////////
	inline void			SetElements(T *pData,int nSize);
	inline void			GetElements(T *pData,int nSize)  const ;
	inline void			GetElementBuf(T *&pData,int &nSize)  const ;

	inline void			SetElement(T& data,int iRow,int jCol);
	inline const T&		GetElement(int iRow,int jCol)  const ;
	inline		 T&		GetElement(int iRow,int jCol);

	inline const T*		operator[](int iRow) const;
	inline		 T*		operator[](int iRow);

	inline		 T&		operator()(int iRow,int jCol);
	inline const T&		operator()(int iRow,int jCol) const;

protected:
	void				Release(void);		

protected:
	int					m_nCol,m_nRow;
	T *					m_pData;
};

//////////////////////////////////////////////////////////////////////////
template<class T> 
Matrix2D<T>::Matrix2D(void):m_pData(NULLPTR)
,m_nRow(0)
,m_nCol(0)
{

}

template<class T> 
Matrix2D<T>::Matrix2D(int nRow, int nCol):m_pData(NULLPTR)
,m_nRow(0)
,m_nCol(0)
{
	Resize(nRow,nCol);
}

template<class T>
Matrix2D<T>::~Matrix2D()
{
	Release();
}

//////////////////////////////////////////////////////////////////////////
template<class T>
void Matrix2D<T>::Resize(int nRow,int nCol)
{
	assert((nCol > 0)&&(nRow > 0));

	if (nCol == m_nCol && nRow == m_nRow)
		return;

	Release();

	m_nCol = nCol;
	m_nRow = nRow;
	m_pData = new T[m_nRow*m_nCol];	
	memset(m_pData,0,m_nRow*m_nCol*sizeof(T));
}

template<class T>
void Matrix2D<T>::Release(void)
{
	if(m_pData) 
	{
		delete []m_pData;
		m_pData = NULLPTR;
		m_nCol = 0;
		m_nRow = 0;
	}
}

template<class T>
T*	Matrix2D<T>::operator[](int iRow)
{
	assert(NULLPTR != m_pData && iRow < m_nRow);

	return m_pData + m_nCol*iRow;
}

template<class T>
const T* Matrix2D<T>::operator[](int iRow) const
{
	assert(NULLPTR != m_pData && iRow < m_nRow);

	return m_pData + m_nCol*iRow;
}

template<class T>
inline  T& Matrix2D<T>::operator()(int iRow,int jCol)
{
	assert(NULLPTR != m_pData && 
		iRow < m_nRow && jCol < m_nCol);

	return m_pData[iRow][jCol];
}

template<class T>
inline  const T& Matrix2D<T>::operator()(int iRow,int jCol) const
{
	assert(NULLPTR != m_pData && 
		iRow < m_nRow && jCol < m_nCol);

	return m_pData[iRow][jCol];
}

//////////////////////////////////////////////////////////////////////////
template<class T>
inline void	Matrix2D<T>::CalcMat2DIndex(int &iRow,int &jCol,int index)  const 
{
	assert(index < (m_nRow-1)*(m_nCol-1));

	iRow = index/m_nCol;
	jCol = index%m_nCol;
}

template<class T>
inline int Matrix2D<T>::CalcMat2DIndex(int iRow,int jCol)  const 
{
	return (m_nCol*iRow + jCol);
}

//////////////////////////////////////////////////////////////////////////
template<class T>
inline void Matrix2D<T>::SetElements(T *pData,int nSize)
{
	assert(NULLPTR != m_pData && nSize == (m_nRow*m_nCol));

	memcpy(m_pData,pData,sizeof(T)*nSize);
}

template<class T>
inline void Matrix2D<T>::GetElements(T *pData,int nSize)  const 
{
	assert(NULLPTR != m_pData && nSize == (m_nRow*m_nCol));

	memcpy(pData,m_pData,sizeof(T)*nSize);
}

template<class T>
inline void Matrix2D<T>::GetElementBuf(T *&pData,int &nSize)  const 
{
	nSize = m_nRow*m_nCol;
	pData = m_pData;
}

template<class T>
inline void Matrix2D<T>::SetElement(T& data,int iRow,int jCol)
{
	assert(NULLPTR != m_pData && 
		iRow < m_nRow && jCol < m_nCol);

	m_pData[m_nCol*iRow + jCol] = data;
}

template<class T>
inline const T& Matrix2D<T>::GetElement(int iRow,int jCol)  const 
{
	assert(NULLPTR != m_pData && 
		iRow < m_nRow && jCol < m_nCol);

	return m_pData[m_nCol*iRow + jCol];
}

template<class T>
inline T& Matrix2D<T>::GetElement(int iRow,int jCol)
{
	assert(NULLPTR != m_pData && 
		iRow < m_nRow && jCol < m_nCol);

	return m_pData[m_nCol*iRow + jCol];
}

#endif// _SMT_MATRIX_2D_H