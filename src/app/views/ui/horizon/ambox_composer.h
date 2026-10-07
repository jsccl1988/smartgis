// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef APP_VIEWS_UI_HORIZON_AMBOX_COMPOSER_H_
#define APP_VIEWS_UI_HORIZON_AMBOX_COMPOSER_H_

namespace app {

class BrowserView;

// Map tool bar + right-dock Tools AMBox population / Plugin Manager.
class AmboxComposer {
 public:
  explicit AmboxComposer(BrowserView* host);
  ~AmboxComposer() = default;

  AmboxComposer(const AmboxComposer&) = delete;
  AmboxComposer& operator=(const AmboxComposer&) = delete;

  void populate_ambox();
  void on_plugins();

 private:
  BrowserView* host_ = nullptr;
};

}  // namespace app

#endif  // APP_VIEWS_UI_HORIZON_AMBOX_COMPOSER_H_
