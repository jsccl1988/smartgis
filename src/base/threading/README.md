<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# `base/threading`

mogu 对齐的线程辅助：`base::this_thread` / `base::jthread`。

- **不** port mogu `base::mutex` — 使用 `std::mutex` / `std::shared_mutex`。
- Windows：`GetCurrentThreadId` / `SetThreadDescription`；无裸 `pthread.h`。

Include：`#include "base/threading/thread.h"`。
