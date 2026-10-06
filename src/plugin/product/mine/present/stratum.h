// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef PLUGIN_MINE_PRESENT_STRATUM_H_
#define PLUGIN_MINE_PRESENT_STRATUM_H_

#include <string>

namespace content {
class GisDocument;
class Scene3dPresenter;
}

namespace gis {
namespace detail {
struct StratumTin;
struct BoreholeSet;
}
}

namespace plugin {
class Scene3dSink;

// Map2d TIN + borehole sticks. Volume overlay still needs Scene3dPresenter.
bool present_mine_stratum(content::GisDocument* doc,
                          Scene3dSink* sink,
                          content::Scene3dPresenter* scene3d,
                          const gis::detail::StratumTin& tin,
                          const gis::detail::BoreholeSet& holes,
                          std::string* err);

}  // namespace plugin

#endif  // PLUGIN_MINE_PRESENT_STRATUM_H_
