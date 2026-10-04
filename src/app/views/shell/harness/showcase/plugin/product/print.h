// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef APP_VIEWS_SHELL_HARNESS_SHOWCASE_PLUGIN_PRODUCT_PRINT_H_
#define APP_VIEWS_SHELL_HARNESS_SHOWCASE_PLUGIN_PRODUCT_PRINT_H_

namespace app {

class Browser;

namespace detail {

// China map + PrintComposer layout chrome (legend/scale) HWND/file BMP.
int run_print(Browser& browser);

}  // namespace detail
}  // namespace app

#endif  // APP_VIEWS_SHELL_HARNESS_SHOWCASE_PLUGIN_PRODUCT_PRINT_H_
