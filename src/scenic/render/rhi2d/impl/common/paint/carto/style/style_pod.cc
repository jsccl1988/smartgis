// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "scenic/render/rhi2d/impl/common/paint/carto/style/style_pod.h"

namespace scenic {
namespace detail {

Style::Style() {
  std::strcpy(name_, "Default");
  type_ = kStPenDesc;
}

Style::Style(const char* name, const PenDesc& pen, const BrushDesc& brush,
             const AnnotationDesc& anno, const SymbolDesc& symbol)
    : pen_(pen), brush_(brush), anno_(anno), symbol_(symbol) {
  set_style_name(name);
}

Style::~Style() = default;

Style* Style::clone(const char* new_name) const {
  Style* out = new Style(new_name, pen_, brush_, anno_, symbol_);
  out->set_style_type(type_);
  return out;
}

}  // namespace detail
}  // namespace scenic
