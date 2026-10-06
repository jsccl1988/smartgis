// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef APP_VIEWS_UI_PANELS_ATMOSPHERE_COMPOSER_H_
#define APP_VIEWS_UI_PANELS_ATMOSPHERE_COMPOSER_H_

#include <functional>
#include <string>

namespace app {

class BrowserView;

// Atmosphere inspector panel horizon wire.
class AtmosphereComposer {
 public:
  explicit AtmosphereComposer(BrowserView* host);
  ~AtmosphereComposer() = default;

  AtmosphereComposer(const AtmosphereComposer&) = delete;
  AtmosphereComposer& operator=(const AtmosphereComposer&) = delete;

  void wire_atmosphere_panel();

 private:
  BrowserView* host_ = nullptr;
};

}  // namespace app

#endif  // APP_VIEWS_UI_PANELS_ATMOSPHERE_COMPOSER_H_
