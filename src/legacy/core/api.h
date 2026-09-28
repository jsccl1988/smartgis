// Copyright (c) 2010 CCL. All rights reserved.
#ifndef _SMT_API_H
#define _SMT_API_H

#include "base/core/export.h"
#include "legacy/core/bas_struct.h"
#include "legacy/core/core.h"
#include "legacy/core/listener.h"

static const double dEPSILON = 1E-5;
static const double dPI = 3.1415926;

bool BASE_EXPORT var_to_bool(const base::SmtVariant& var);
byte BASE_EXPORT var_to_byte(const base::SmtVariant& var);
short BASE_EXPORT var_to_integer(const base::SmtVariant& var);
long BASE_EXPORT var_to_long(const base::SmtVariant& var);
double BASE_EXPORT var_to_double(const base::SmtVariant& var);
const char BASE_EXPORT* var_to_string(const base::SmtVariant& var);

int BASE_EXPORT integer_list_to_string(int nCount, int* pInteger,
                                       int nStingBufLength = TEMP_BUFFER_SIZE,
                                       char* buf = NULL);
int BASE_EXPORT string_list_to_string(int nCount, char** pStrings,
                                      int nStingBufLength = TEMP_BUFFER_SIZE,
                                      char* buf = NULL);
int BASE_EXPORT real_list_to_string(int nCount, double* pReal,
                                    int nStingBufLength = TEMP_BUFFER_SIZE,
                                    char* buf = NULL);

int BASE_EXPORT str_count(char** papszStrList);
char BASE_EXPORT** str_duplicate(char** papszStrList);
uint BASE_EXPORT str_tokenize(const string& str, vector<string>& tokens,
                              const string& delimiters);
bool BASE_EXPORT is_equal(double a, double b, double eps);

void BASE_EXPORT split_file_name(const char* fullname, char* path,
                                 char* fileName, char* title, char* ext);
void BASE_EXPORT get_parent_directory(const char* szCurDir, char* szParentDir,
                                      int iParent = 1);
string BASE_EXPORT get_app_path(void);
string BASE_EXPORT get_app_temp_path(void);
inline string GetAppPath(void) { return get_app_path(); }
inline string GetAppTempPath(void) { return get_app_temp_path(); }

long BASE_EXPORT create_all_path_directory(string strDirPath);
long BASE_EXPORT delete_directory(string strDirName);
long BASE_EXPORT get_temp_name(string& strName);

void BASE_EXPORT l_rect_to_f_rect(base::fRect& frect, base::lRect lrect);
void BASE_EXPORT f_rect_to_l_rect(base::lRect& lrect, base::fRect frect);

void BASE_EXPORT adjust_l_rect(base::lRect& lrect);
void BASE_EXPORT adjust_f_rect(base::fRect& frect);

bool BASE_EXPORT is_in_f_rect(float x, float y, base::fRect& frect);
bool BASE_EXPORT is_in_l_rect(long x, long y, base::lRect& lrect);

long BASE_EXPORT get_interp_color(long index, long internum, long r1, long g1,
                                  long b1, long r2, long g2, long b2);
long BASE_EXPORT get_random_color(void);

HMENU BASE_EXPORT create_listener_menu(base::SmtListener* pListener,
                                       base::SmtFuncItemStyle style);
void BASE_EXPORT append_listener_menu(HMENU hOwnwerMenu,
                                      base::SmtListener* pListener,
                                      base::SmtFuncItemStyle style,
                                      bool bInsertSeprator = true);
// HMENU is pointer-sized. Passing it through UINT truncates on x64 and
// yields an invalid submenu (AV in TrackPopupMenu / DrawMenuBar).
bool BASE_EXPORT append_popup_menu(HMENU owner, HMENU popup, const char* name);
bool BASE_EXPORT insert_popup_menu(HMENU owner, UINT position, HMENU popup,
                                   const char* name, UINT extra_flags);
// Owns the menu from create_listener_menu. insert_at < 0 appends.
bool BASE_EXPORT attach_listener_popup(HMENU owner, base::SmtListener* listener,
                                       base::SmtFuncItemStyle style,
                                       const char* name, int insert_at = -1,
                                       UINT extra_flags = 0);

long BASE_EXPORT get_image_type_by_file_ext(const char* szFileName);

#if !defined(BASE_EXPORTS)
#if defined(_DEBUG)
#pragma comment(lib, "base_d.lib")
#else
#pragma comment(lib, "base.lib")
#endif
#endif

#endif  //_SMT_API_H