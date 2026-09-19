/*
File:    bl_style.h

Desc:    SmtStyle,SmtStyleTable

Version: Version 1.0

Writter:  �´���

Date:    2010.11.17

Copyright (c) 2026 The Mogu Authors.
All rights reserved.
*/
#ifndef _BL_STYLE_H
#define _BL_STYLE_H

#include "base/core/core.h"

#if defined(STYLE_EXPORTS)
#define STYLE_EXPORT __declspec(dllexport)
#else
#define STYLE_EXPORT __declspec(dllimport)
#endif
#include "base/core/bas_struct.h"
#include "base/core/env_struct.h"

namespace base {
enum StType {
  ST_PenDesc = 0x0001,
  ST_BrushDesc = 0x0002,
  ST_AnnoDesc = 0x0004,
  ST_SymbolDesc = 0x008
};

struct SmtPenDesc {
  long lPenColor;
  long lPenStyle;
  float fPenWidth;

  SmtPenDesc(void) {
    lPenColor = RGB(0, 255, 0);
    lPenStyle = PS_SOLID;
    fPenWidth = 0.002;
  }
};

struct SmtBrushDesc {
  enum brushType { BT_Solid, BT_Hatch } brushTp;

  long lBrushColor;
  long lBrushStyle;

  SmtBrushDesc(void) {
    brushTp = BT_Solid;
    lBrushColor = RGB(0, 255, 255);
    lBrushStyle = HS_FDIAGONAL;
  }
};

struct SmtAnnotationDesc {
  float fHeight;
  float fWidth;
  long lEscapement;
  long lOrientation;
  long lWeight;
  byte lItalic;
  byte lUnderline;
  byte lStrikeOut;
  byte lCharSet;
  byte lOutPrecision;
  byte lClipPrecision;
  byte lQuality;
  byte lPitchAndFamily;
  char szFaceName[32];

  long lAnnoClr;
  float fAngle;
  float fSpace;

  SmtAnnotationDesc(void) {
    fHeight = 1.6;
    fWidth = 1.6;
    lEscapement = 0;
    lOrientation = 0;
    lWeight = 50;
    lItalic = FALSE;
    lUnderline = FALSE;
    lStrikeOut = 0;
    lCharSet = DEFAULT_CHARSET;
    lOutPrecision = OUT_TT_PRECIS;
    lClipPrecision = CLIP_CHARACTER_PRECIS;
    lQuality = DEFAULT_QUALITY;
    lPitchAndFamily = FIXED_PITCH;
    strcpy(szFaceName, "Times new_ Roman");

    lAnnoClr = RGB(0, 0, 0);
    fAngle = 0;
    fSpace = 0.2;
  }
};

struct SmtSymbolDesc {
  long lSymbolID;
  float fSymbolWidth;
  float fSymbolHeight;

  SmtSymbolDesc(void) {
    lSymbolID = 0;
    fSymbolHeight = 0.4;
    fSymbolWidth = 0.4;
  }
};

class STYLE_EXPORT SmtStyle {
 public:
  SmtStyle(void);
  SmtStyle(const char *szName, const SmtPenDesc &penDesc,
           const SmtBrushDesc &brushDesc, const SmtAnnotationDesc &annoDesc,
           const SmtSymbolDesc &symbolDesc);

  ~SmtStyle();

 public:
  const char *get_style_name() const { return m_szName; }
  ulong get_style_type(void) const { return m_stType; }

  void set_style_type(ulong tp) { m_stType = tp; }
  void set_style_name(const char *szName) { strcpy(m_szName, szName); }
  SmtStyle *clone(const char *szNewName) const;

  //////////////////////////////////////////////////////////////////////////
  inline void set_pen_desc(const SmtPenDesc &penDesc) { m_stPenDesc = penDesc; }
  inline void set_brush_desc(const SmtBrushDesc &brushDesc) {
    m_stBrushDesc = brushDesc;
  }
  inline void set_anno_desc(const SmtAnnotationDesc &annoDesc) {
    m_stAnnoDesc = annoDesc;
  }
  inline void set_symbol_desc(const SmtSymbolDesc &symbolDesc) {
    m_stSymbolDesc = symbolDesc;
  }

  //////////////////////////////////////////////////////////////////////////
  inline SmtPenDesc &get_pen_desc(void) { return m_stPenDesc; }
  inline SmtBrushDesc &get_brush_desc(void) { return m_stBrushDesc; }
  inline SmtAnnotationDesc &get_anno_desc(void) { return m_stAnnoDesc; }
  inline SmtSymbolDesc &get_symbol_desc(void) { return m_stSymbolDesc; }

  inline const SmtPenDesc &get_pen_desc(void) const { return m_stPenDesc; }
  inline const SmtBrushDesc &get_brush_desc(void) const {
    return m_stBrushDesc;
  }
  inline const SmtAnnotationDesc &get_anno_desc(void) const {
    return m_stAnnoDesc;
  }
  inline const SmtSymbolDesc &get_symbol_desc(void) const {
    return m_stSymbolDesc;
  }

 protected:
  char m_szName[MAX_STYLENAME_LENGTH];
  ulong m_stType;

  SmtPenDesc m_stPenDesc;
  SmtBrushDesc m_stBrushDesc;
  SmtAnnotationDesc m_stAnnoDesc;
  SmtSymbolDesc m_stSymbolDesc;
};

typedef vector<SmtStyle *> StylePtrList;

class STYLE_EXPORT SmtStyleTable {
 public:
  SmtStyleTable(void);
  ~SmtStyleTable(void);

  inline int get_style_count(void) const { return m_nStyleCount; }

  SmtStyleTable *clone(void) const;

  int add_style(const char *stylename);
  void remove_style(const char *stylename);

  SmtStyle *get_style(const char *stylename);
  const SmtStyle *get_style(const char *stylename) const;

  SmtStyle *get_style(int index);
  const SmtStyle *get_style(int index) const;

  const char *get_style_name(int index);

 protected:
  int find_style_name_index(const char *stylename) const;

 protected:
  char **m_pStyleNames;
  int m_nStyleCount;
};
}  // namespace base

#if !defined(STYLE_EXPORTS)
#if defined(_DEBUG)
#pragma comment(lib, "platform_d.lib")
#else
#pragma comment(lib, "platform.lib")
#endif
#endif

#endif  //_BL_STYLE_H