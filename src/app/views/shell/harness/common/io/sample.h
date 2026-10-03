// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef APP_VIEWS_SHELL_HARNESS_COMMON_IO_SAMPLE_H_
#define APP_VIEWS_SHELL_HARNESS_COMMON_IO_SAMPLE_H_

namespace app {

class Browser;

namespace detail {

// Opens china_city / china_plp sample under out/data (or already-loaded doc).
// When |write_stub_if_missing| is true, writes a tiny GeoJSON stub on miss
// (console self-test path).
bool try_open_china_sample(Browser& browser, bool write_stub_if_missing);

}  // namespace detail
}  // namespace app

#endif  // APP_VIEWS_SHELL_HARNESS_COMMON_IO_SAMPLE_H_
