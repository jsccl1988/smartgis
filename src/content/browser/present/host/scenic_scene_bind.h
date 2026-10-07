// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef CONTENT_BROWSER_PRESENT_HOST_SCENIC_SCENE_BIND_H_
#define CONTENT_BROWSER_PRESENT_HOST_SCENIC_SCENE_BIND_H_

#include <vector>

#include "scenic/engine.h"

namespace content {

class GisScene;
class OrbitFrame;
class ViewFrame;

namespace detail {

// Copies visible GisScene features into Scenic POD (non-owning DrawItem.xy
// aliases |xy|). |xy| / |items| are cleared then filled.
void fill_scenic_draw_items(const GisScene* scene, double scale,
                            std::vector<scenic::Vertex2>* xy,
                            std::vector<scenic::DrawItem>* items);

scenic::ViewXform scenic_view_from_frame(const ViewFrame* frame);
scenic::OrbitXform scenic_orbit_from_host(const OrbitFrame* orbit,
                                          const GisScene* scene);

}  // namespace detail
}  // namespace content

#endif  // CONTENT_BROWSER_PRESENT_HOST_SCENIC_SCENE_BIND_H_
