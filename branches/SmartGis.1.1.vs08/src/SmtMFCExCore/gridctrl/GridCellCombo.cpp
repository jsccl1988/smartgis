// GridCellCombo.cpp : implementation file
//
// MFC Grid Control - Main grid cell class
//
// Provides the implementation for a combobox cell type of the
// grid control.
//
// Written by Chris Maunder <cmaunder@mail.com>
// Copyright (c) 1998-2001. All Rights Reserved.
//
// Parts of the code contained in this file are based on the original
// CInPlaceList from http://www.codeguru.com/listview
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
// For use with CGridCtrl v2.22+
//
// History:
// 6 Aug 1998 - Added CComboEdit to subclass the edit control - code 
//              provided by Roelf Werkman <rdw@inn.nl>. Added nID to 
//              the constructor param list.
// 29 Nov 1998 - bug fix in onkeydown (Markus Irtenkauf)
//
/////////////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "GridCell.h"
#include "GridCtrl.h"

#include "GridCellCombo.h"


#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CGridCellCombo 
/////////////////////////////////////////////////////////////////////////////


IMPLEMENT_DYNCREATE(CGridCellCombo, CGridCell)

CGridCellCombo::CGridCellCombo() : CGridCell()
{
    SetStyle(CBS_DROPDOWN);  // CBS_DROPDOWN, CBS_DROPDOWNLIST, CBS_SIMPLE, CBS_SORT
}

// Create a control to do the editing
BOOL CGridCellCombo::Edit(int nRow, int nCol, CRect rect, CPoint /* point */, UINT nID, UINT nChar)
{
    m_bEditing = TRUE;
    
    // CInPlaceList auto-deletes itself
    m_pEditWnd = new CInPlaceList(GetGrid(), rect, GetStyle(), nID, nRow, nCol, 
                                  GetTextClr(), GetBackClr(), m_Strings, GetText(), nChar);

    return TRUE;
}

CWnd* CGridCellCombo::GetEditWnd() const
{
	if (m_pEditWnd && (m_pEditWnd->GetStyle() & CBS_DROPDOWNLIST) != CBS_DROPDOWNLIST )
		return &(((CInPlaceList*)m_pEditWnd)->m_edit);

	return NULL;
}

CSize CGridCellCombo::GetCellExtent(CDC* pDC)
{
    CSize sizeScroll(GetSystemMetrics(SM_CXVSCROLL), GetSystemMetrics(SM_CYHSCROLL));

	return CGridCell::GetCellExtent(pDC) + sizeScroll;
}

// Cancel the editing.
void CGridCellCombo::EndEdit()
{
    if (m_pEditWnd)
        ((CInPlaceList*)m_pEditWnd)->EndEdit();
}

// Override draw so that when the cell is selected, a drop arrow is shown in the RHS.
BOOL CGridCellCombo::Draw(CDC* pDC, int nRow, int nCol, CRect rect,  BOOL bEraseBkgnd /*=TRUE*/)
{
#ifdef _WIN32_WCE
    return CGridCell::Draw(pDC, nRow, nCol, rect,  bEraseBkgnd);
#else
    // Cell selected?
    //if ( !IsFixed() && IsFocused())
    if (GetGrid()->IsCellEditable(nRow, nCol) && !IsEditing())
    {
        // Get the size of the scroll box
        CSize sizeScroll(GetSystemMetrics(SM_CXVSCROLL), GetSystemMetrics(SM_CYHSCROLL));

        // enough room to draw?
        if (sizeScroll.cy < rect.Width() && sizeScroll.cy < rect.Height())
        {
            // Draw control at RHS of cell
            CRect ScrollRect = rect;
            ScrollRect.left   = rect.right - sizeScroll.cx;
            ScrollRect.bottom = rect.top + sizeScroll.cy;

            // Do the draw 
            pDC->DrawFrameControl(ScrollRect, DFC_SCROLL, DFCS_SCROLLDOWN);

            // Adjust the remaining space in the cell
            rect.right = ScrollRect.left;
        }
    }

    CString strTempText = GetText();
    if (IsEditing())
        SetText(_T(""));

    // drop through and complete the cell drawing using the base class' method
    BOOL bResult = CGridCell::Draw(pDC, nRow, nCol, rect,  bEraseBkgnd);

    if (IsEditing())
        SetText(strTempText);

	return bResult;
#endif
}

// For setting the strings that will be displayed in the drop list
void CGridCellCombo::SetOptions(CStringArray& ar)
{ 
    m_Strings.RemoveAll();
    for (int i = 0; i < ar.GetSize(); i++)
        m_Strings.Add(ar[i]);
}