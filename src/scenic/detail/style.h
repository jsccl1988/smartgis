// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef SCENIC_DETAIL_STYLE_H_
#define SCENIC_DETAIL_STYLE_H_

#include <cstring>
#include <vector>

#include "scenic/detail/err.h"
#include "scenic/detail/geom.h"
#include "scenic/scenic_impl_export.h"

#ifndef MAX_STYLENAME_LENGTH
#define MAX_STYLENAME_LENGTH 50
#endif

// Scenic-owned cartographic style POD (pen/brush/anno/symbol). Replaces
// legacy/gis/present/carto/style.h for scenic_copy TUs.

namespace scenic {
namespace detail {

enum StyleTypeFlag {
  kStPenDesc = 0x0001,
  kStBrushDesc = 0x0002,
  kStAnnoDesc = 0x0004,
  kStSymbolDesc = 0x0008,
};

struct PenDesc {
  long lPenColor = RGB(0, 255, 0);
  long lPenStyle = PS_SOLID;
  float fPenWidth = 0.002f;
};

struct BrushDesc {
  // Historical enumerator names kept for style.cc / GDI hatch paths.
  enum brushType { BT_Solid = 0, BT_Hatch = 1 };
  brushType brushTp = BT_Solid;
  long lBrushColor = RGB(0, 255, 255);
  long lBrushStyle = HS_FDIAGONAL;
};

struct AnnotationDesc {
  float fHeight = 1.6f;
  float fWidth = 1.6f;
  long lEscapement = 0;
  long lOrientation = 0;
  long lWeight = 50;
  byte lItalic = FALSE;
  byte lUnderline = FALSE;
  byte lStrikeOut = 0;
  byte lCharSet = DEFAULT_CHARSET;
  byte lOutPrecision = OUT_TT_PRECIS;
  byte lClipPrecision = CLIP_CHARACTER_PRECIS;
  byte lQuality = DEFAULT_QUALITY;
  byte lPitchAndFamily = FIXED_PITCH;
  char szFaceName[32]{};
  long lAnnoClr = RGB(0, 0, 0);
  float fAngle = 0.f;
  float fSpace = 0.2f;

  AnnotationDesc() { std::strcpy(szFaceName, "Times new_ Roman"); }
};

struct SymbolDesc {
  long lSymbolID = 0;
  float fSymbolWidth = 0.4f;
  float fSymbolHeight = 0.4f;
};

// Leftover-compatible carto style (not Style JSON).
class LEGACY_RENDER_EXPORT Style {
 public:
  Style();
  Style(const char* name, const PenDesc& pen, const BrushDesc& brush,
        const AnnotationDesc& anno, const SymbolDesc& symbol);
  ~Style();

  const char* get_style_name() const { return name_; }
  ulong get_style_type() const { return type_; }
  void set_style_type(ulong tp) { type_ = tp; }
  void set_style_name(const char* name) {
    if (name) {
      strncpy(name_, name, MAX_STYLENAME_LENGTH - 1);
      name_[MAX_STYLENAME_LENGTH - 1] = '\0';
    }
  }
  Style* clone(const char* new_name) const;

  void set_pen_desc(const PenDesc& pen) { pen_ = pen; }
  void set_brush_desc(const BrushDesc& brush) { brush_ = brush; }
  void set_anno_desc(const AnnotationDesc& anno) { anno_ = anno; }
  void set_symbol_desc(const SymbolDesc& symbol) { symbol_ = symbol; }

  PenDesc& get_pen_desc() { return pen_; }
  BrushDesc& get_brush_desc() { return brush_; }
  AnnotationDesc& get_anno_desc() { return anno_; }
  SymbolDesc& get_symbol_desc() { return symbol_; }
  const PenDesc& get_pen_desc() const { return pen_; }
  const BrushDesc& get_brush_desc() const { return brush_; }
  const AnnotationDesc& get_anno_desc() const { return anno_; }
  const SymbolDesc& get_symbol_desc() const { return symbol_; }

 private:
  char name_[MAX_STYLENAME_LENGTH] = "Default";
  ulong type_ = kStPenDesc;
  PenDesc pen_;
  BrushDesc brush_;
  AnnotationDesc anno_;
  SymbolDesc symbol_;
};

}  // namespace detail
}  // namespace scenic

// Historical names in namespace base (rhi2d `using namespace base`).
namespace base {
using StType = ::scenic::detail::StyleTypeFlag;
inline constexpr StType ST_PenDesc = ::scenic::detail::kStPenDesc;
inline constexpr StType ST_BrushDesc = ::scenic::detail::kStBrushDesc;
inline constexpr StType ST_AnnoDesc = ::scenic::detail::kStAnnoDesc;
inline constexpr StType ST_SymbolDesc = ::scenic::detail::kStSymbolDesc;

using PenDesc = ::scenic::detail::PenDesc;
using BrushDesc = ::scenic::detail::BrushDesc;
using AnnotationDesc = ::scenic::detail::AnnotationDesc;
using SymbolDesc = ::scenic::detail::SymbolDesc;
using Style = ::scenic::detail::Style;
}  // namespace base

#endif  // SCENIC_DETAIL_STYLE_H_
