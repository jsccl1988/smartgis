// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef APP_VIEWS_SHELL_UI_PANELS_ATMOSPHERE_CHROME_H_
#define APP_VIEWS_SHELL_UI_PANELS_ATMOSPHERE_CHROME_H_

#include <functional>
#include <string>

namespace app {

class BrowserView;

// Atmosphere inspector panel chrome wire.
class AtmosphereChrome {
 public:
  explicit AtmosphereChrome(BrowserView* host);
  ~AtmosphereChrome() = default;

  AtmosphereChrome(const AtmosphereChrome&) = delete;
  AtmosphereChrome& operator=(const AtmosphereChrome&) = delete;

  void wire_atmosphere_panel();

 private:
  BrowserView* host_ = nullptr;
};

}  // namespace app

#endif  // APP_VIEWS_SHELL_UI_PANELS_ATMOSPHERE_CHROME_H_
