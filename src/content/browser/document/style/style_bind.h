// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef CONTENT_BROWSER_DOCUMENT_STYLE_STYLE_BIND_H_
#define CONTENT_BROWSER_DOCUMENT_STYLE_STYLE_BIND_H_

#include <cstdint>
#include <memory>
#include <string>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include "content/browser/document/store/gis_layer.h"
#include "gis/style/style_types.h"
#include "gis/tile/provider/tile_provider.h"

namespace content {
namespace detail {

// Owns optional StyleDocument + basemap TileProvider for GisScene paint resolve.
class StyleBind {
 public:
  void set_style_document(std::shared_ptr<gis::style::StyleDocument> doc);
  void clear_style_document();
  bool load_style_path(const std::string& path);
  bool has_style_document() const { return static_cast<bool>(style_doc_); }
  const gis::style::StyleDocument* style_document() const {
    return style_doc_.get();
  }
  std::shared_ptr<gis::style::StyleDocument> style_document_shared() const {
    return style_doc_;
  }

  void set_basemap_provider(std::shared_ptr<gis::tile::TileProvider> provider);
  void clear_basemap_provider();
  bool has_basemap_provider() const {
    return basemap_ && basemap_->is_open();
  }
  gis::tile::TileProvider* basemap_provider() const { return basemap_.get(); }

  bool resolve_style_for_test(const std::string& source_layer,
                              const gis::style::AttrMap& attrs, double zoom,
                              gis::style::ResolvedPaint* out) const;

  bool style_colors_for_feature(const GisLayer& layer, const GisFeature& f,
                                double scale, COLORREF* fill, COLORREF* stroke,
                                int* stroke_width) const;

 private:
  std::shared_ptr<gis::style::StyleDocument> style_doc_;
  std::shared_ptr<gis::tile::TileProvider> basemap_;
};

double style_zoom_from_scale(double scale);
COLORREF argb_to_colorref(uint32_t argb);
std::string argb_to_hex_rgb(uint32_t argb);
std::string colorref_to_hex(COLORREF c);
const gis::style::StyleDocument* embedded_carto_style_document();
bool read_file_bytes(const std::string& path, std::string* out);

}  // namespace detail
}  // namespace content

#endif  // CONTENT_BROWSER_DOCUMENT_STYLE_STYLE_BIND_H_
