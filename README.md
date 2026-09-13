<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

SmartGIS 是一段还没收尾的桌面 GIS 故事：十年前它靠 MFC 与一堆 `Smt_*` DLL 在真地图上干活；如今工程入口已换成 mogu 式 GN，源码收进 `src/` 分层，壳的终局钉死在 Views + Skia——地图仍挂在自家 C++ viewport 上，不走 Qt，Feature Pack 只是能编通的桥。我们一边守住 LoadLibrary 边界，一边把新树切向当代 C++；旧 ABI 与新层并存，不是怀旧，是迁徙还没走完。
