#if !defined(AFX_GRIDCELLCOMBO_H__ECD42822_16DF_11D1_992F_895E185F9C72__INCLUDED_)
#define AFX_GRIDCELLCOMBO_H__ECD42822_16DF_11D1_992F_895E185F9C72__INCLUDED_

#if _MSC_VER >= 1000
#pragma once
#endif // _MSC_VER >= 1000

/////////////////////////////////////////////////////////////////////////////
// GridCellCombo.h : header file
//
// MFC Grid Control - Grid combo cell class header file
//
// Written by Chris Maunder <cmaunder@mail.com>
// Copyright (c) 1998-2002. All Rights Reserved.
//
// This code may be used in compiled form in any way you desire. This
// file may be redistributed unmodified by any means PROVIDING it is 
// not sold for profit without the authors written consent, and 
// providing that this notice and the authors name and all copyright 
// notices remains intact. 
//
// An email letting me know how you are using it would be nice as well. 
//
// This file is provided "as is" with no expressed or implied warranty.
// The author accepts no liability for any damage/loss of business that
// this product may cause.
//
// For use with CGridCtrl v2.10
//
//////////////////////////////////////////////////////////////////////


#include "GridCell.h"
#include "InPlaceList.h"

class AFX_EXT_CLASS CGridCellCombo : public CGridCell
{
    friend class CGridCtrl;
    DECLARE_DYNCREATE(CGridCellCombo)

public:
    CGridCellCombo();

// editing cells
public:
    virtual BOOL  Edit(int nRow, int nCol, CRect rect, CPoint point, UINT nID, UINT nChar);
    virtual CWnd* GetEditWnd() const;
    virtual void  EndEdit();

// Operations
public:
	virtual CSize GetCellExtent(CDC* pDC);

// CGridCellCombo specific calls
public:
    void  SetOptions(CStringArray& ar);
    void  SetStyle(DWORD dwStyle)           { m_dwStyle = dwStyle; }
    DWORD GetStyle()                        { return m_dwStyle;    }

protected:
    virtual BOOL Draw(CDC* pDC, int nRow, int nCol, CRect rect, BOOL bEraseBkgnd = TRUE);

    CStringArray m_Strings;
    DWORD        m_dwStyle;
};
/////////////////////////////////////////////////////////////////////////////

//{{AFX_INSERT_LOCATION}}
// Microsoft Developer Studio will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_GRIDCELLCOMBO_H__ECD42822_16DF_11D1_992F_895E185F9C72__INCLUDED_)
