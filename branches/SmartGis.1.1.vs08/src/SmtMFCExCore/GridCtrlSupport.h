#ifndef _GRIDCTRL_SUPPORT_H
#define _GRIDCTRL_SUPPORT_H

#include "CellRange.h"
#include "GridCell.h"
#include "GridBtnCellBase.h"
#include "GridBtnCell.h"
#include "GridBtnCellCombo.h"
#include "GridCellCheck.h"
#include "GridCellCombo.h"
#include "GridCellDateTime.h"
#include "GridCellNumeric.h"
#include "GridURLCell.h"
#include "GridDropTarget.h"
#include "GridCtrl.h"
#include "InPlaceEdit.h"
#include "InPlaceList.h"
#include "TitleTip.h"

#if !defined(Export_SmtMFCExCore)
#if   defined( _DEBUG)
#          pragma comment(lib,"SmtMFCExCoreD.lib")
#       else
#          pragma comment(lib,"SmtMFCExCore.lib")
#	    endif  
#endif

#endif //_GRIDCTRL_SUPPORT_H