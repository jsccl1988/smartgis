#ifndef _GRIDCTRL_SUPPORT_H
#define _GRIDCTRL_SUPPORT_H
#if defined(MFC_EX_EXPORTS)
#define MFC_EX_EXPORT __declspec(dllexport)
#else
#define MFC_EX_EXPORT __declspec(dllimport)
#endif


#include "ui/mfc_ex/gridctrl/cell_range.h"
#include "ui/mfc_ex/gridctrl/grid_cell.h"
#include "ui/mfc_ex/gridctrl/grid_btn_cell_base.h"
#include "ui/mfc_ex/gridctrl/grid_btn_cell.h"
#include "ui/mfc_ex/gridctrl/grid_btn_cell_combo.h"
#include "ui/mfc_ex/gridctrl/grid_cell_check.h"
#include "ui/mfc_ex/gridctrl/grid_cell_combo.h"
#include "ui/mfc_ex/gridctrl/grid_cell_date_time.h"
#include "ui/mfc_ex/gridctrl/grid_cell_numeric.h"
#include "ui/mfc_ex/gridctrl/grid_url_cell.h"
#include "ui/mfc_ex/gridctrl/grid_drop_target.h"
#include "ui/mfc_ex/gridctrl/grid_ctrl.h"
#include "ui/mfc_ex/gridctrl/in_place_edit.h"
#include "ui/mfc_ex/gridctrl/in_place_list.h"
#include "ui/mfc_ex/gridctrl/title_tip.h"

#if !defined(MFC_EX_EXPORTS)
#if   defined( _DEBUG)
#          pragma comment(lib,"ui_legacy_d.lib")
#       else
#          pragma comment(lib,"ui_legacy.lib")
#	    endif  
#endif

#endif //_GRIDCTRL_SUPPORT_H