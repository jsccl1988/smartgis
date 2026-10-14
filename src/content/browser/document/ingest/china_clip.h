// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef CONTENT_BROWSER_DOCUMENT_INGEST_CHINA_CLIP_H_
#define CONTENT_BROWSER_DOCUMENT_INGEST_CHINA_CLIP_H_

#include <string>

namespace content {
namespace detail {

// True when |path| stem looks like the china_city sample pack.
// Geometry clip / land-mask / OMT class normalize live offline in
// testing/data/carto_prep (see build_china_city.py).
bool path_looks_like_china_city(const std::string& path);

}  // namespace detail
}  // namespace content

#endif  // CONTENT_BROWSER_DOCUMENT_INGEST_CHINA_CLIP_H_
