// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef LEGACY_APP_VIEWS_SELF_TEST_MARK_H_
#define LEGACY_APP_VIEWS_SELF_TEST_MARK_H_

#pragma once

namespace legacy_app {
namespace helper {

// When --self-test is on the command line: append view-ok mark and arm a
// detached TerminateProcess watchdog (MDI/BCG hang avoidance).
void arm_self_test_view_watchdog_if_requested();

}  // namespace helper
}  // namespace legacy_app

#endif  // LEGACY_APP_VIEWS_SELF_TEST_MARK_H_
