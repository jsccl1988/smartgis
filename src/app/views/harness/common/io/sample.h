// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef APP_VIEWS_HARNESS_COMMON_IO_SAMPLE_H_
#define APP_VIEWS_HARNESS_COMMON_IO_SAMPLE_H_

namespace app {

class Browser;

namespace detail {

// Opens china_city / china_plp under out/data (or already-loaded doc).
// Hard-fails when real packs are missing — never writes a GeoJSON stub.
// |write_stub_if_missing| is ignored (kept for CapabilityHost ABI callers).
bool try_open_china_sample(Browser& browser, bool write_stub_if_missing);

}  // namespace detail
}  // namespace app

#endif  // APP_VIEWS_HARNESS_COMMON_IO_SAMPLE_H_
