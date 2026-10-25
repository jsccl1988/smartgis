<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# build â?GN/Ninja toolchain

å¯¹é½ mogu `build/`ï¼ç» mgis ç?Windows ééï¼ï¼å¨å± argsãtoolchainãé»è®?`config`ãæ¥å¸¸å¥å£æ¯ **MSVC v145**ã?
## èè´£

ä»»ä½ compiler/linker éé¡¹ãå¹³å°åæ¢ãç¬¬ä¸æ¹ **è·¯å¾çº¦å®** åºå¨æ­¤ä¿®æ¹ï¼èä¸æ¯åè¿åä¸ªæ¨¡å?`BUILD.gn`ã?
## ç®å½

| Path | è¯´æ |
| --- | --- |
| `BUILDCONFIG.gn` | `default_toolchain`ã`is_debug` / `is_build_third_party` / `cc_std`ï¼Windows **`c++23`** â?MSVC `/std:c++23preview` on 14.50/14.51, `/std:c++23` when 14.52 shipsï¼Linux/mac `c++23`ï¼ãé»è®?`configs` |
| `config/BUILD.gn` | `default`ã`c_std`ã`cc_std`ãå¯é?`warnings` |
| `config/win/` | MSVC é»è®¤ flagsãCRTãsubsystem |
| `BUILD.gn` | `legacy`ï¼MBCS + 2010 include æ ï¼ |
| `smartgis.gni` | `product_shared_library` æ¨¡æ¿ |
| `toolchain/` | win/linux/mac toolchains |
| `tools/` | `cmake.gni` / `makefile.gni` / Python helpers; `check_out_artifacts.py` L0 writable + L1 ninja + L2 PE + L2b DLL homogeneity; `check_dll_homogeneity.py` (Debug `_d` vs Release stems + `SMARTGIS_BUILD_ID` marker); `gen_build_id.py` (`//build:gen_build_id`) |
| `fetch_binaries.py` | æå `gn` / `ninja` å?`build/bin` |

## å¸¸ç¨æä½

```bat
build.bat
build.bat debug
build.bat release
build.bat debug te
```

é»è®¤ **åæ¶** gen/ninja **`out/Debug`**ï¼`is_debug=true`ï¼ä¸ **`out/Release`**ï¼`is_debug=false`ï¼ã`build.bat debug|release â¦` åªç¼ä¸å¥ã`te` / `e2e` è·æµåªä½¿ç?**Debug** äº§ç©ã?
```bat
gn gen out/Debug --args="is_debug=true is_build_third_party=false"
gn args out/Debug --list
ninja -C out/Debug all
```

å¥å£èæ¬ï¼ä»åºæ ¹ `build.bat`ï¼å¯å¸?`debug` / `release`ã`m` / `te` / `a` / `b` / `app` / `views`ï¼ãææ¡£ç´¢å¼ï¼[`docs/README.md`](../docs/README.md)ãäº§ç©çº¦å®ï¼[`.cursor/rules/build/build-output.mdc`](../.cursor/rules/build/build-output.mdc)ã?
### å¤å¯¹è¯ç¼è¯é

`build.bat` ç»?`build/config/win/with_build_lock.ps1` **åªéç¼è¯**ï¼`gn`/`ninja`ãä»¥å?`t`ï¼ã`te` / `e2e` **è·æµå¨æ¾éä¹å?*ã?
| é?| è¦ç |
| --- | --- |
| `out/.build.lock.debug` | `out/Debug` ninja |
| `out/.build.lock.release` | `out/Release` ninja |
| `out/.build.lock.shared` | `build.bat t`ï¼ä¸ä»»ä¸ config äºæ¥ï¼?|
| `out/.build.lock` | å½å holder æè¦ï¼éäºæ¥æ¬ä½ï¼?|

è°åº¦ï¼æ¯ scope **å¬å¹³ ticket éå**ï¼`out/.build.waiters/`ï¼ï¼å¤ç®æ èªå?*æç®æ ææé**ï¼å ticket ä¿ä¼åçº§ï¼ï¼`debug` â?`release` å¯å¹¶è¡ãæåç¼è¯å `out/.build.last_ok`ã?
**å¨å±å¼å³ï¼é»è®¤å³éï¼åäººæ´å¿«ï¼ï¼?*

| å¼å?| ææ |
| --- | --- |
| *(é»è®¤)* | **å?* |
| `out/.build.lock.on` | å¨å±å¼éï¼å æä»¶æ¢å¤é»è®¤å³ï¼?|
| `SMARTGIS_BUILD_LOCK=1` | æ¬è¿ç¨å¼é?|
| `SMARTGIS_BUILD_LOCK=0` | å¼ºå¶å³éï¼è¦ç?sentinelï¼?|

å³éåï¼æ æéãæ æç®æ ï¼ä¸æ¬¡ç´é?`build.bat`ã?
| ç¯å¢åé | å«ä¹ |
| --- | --- |
| `SMARTGIS_BUILD_LOCK` | `1/on` å¼ï¼`0/off` å¼ºå¶å³ï¼æªè®¾åç `out/.build.lock.on`ï¼æ åå³ï¼?|
| `SMARTGIS_BUILD_LOCK_WAIT_SEC` | æé¿ç­å¾ç§æ°ï¼é»è®¤ 1800ï¼`0` = ç«å³å¤±è´¥ï¼?|
| `SMARTGIS_BUILD_OWNER` | å¼éæ¶åå¥æ ç­¾ï¼å»ºè®®å¿å¡«ï¼ |

å¿æ¶éåºç  **3**ãè§èï¼[`.cursor/rules/build/build-lock.mdc`](../.cursor/rules/build/build-lock.mdc)ã?
## ä¸?mogu çå·®å¼ï¼ææä¿çï¼?
- **ä¸?*ä½¿ç¨ Bazel dual-buildãGN å?`third_party/.install`ï¼`build.bat t` â?`third_party/tools/batch.py` è£?prefixï¼å¯¹é½?mogu `build.sh build t`ï¼ã`out/third_party`ï¼åå?config ä¸ç `third_party`ï¼å¯ junction å?`.install`ï¼è¿è¡æ¶æ?DLLï¼?- **ä¸?*æ?sln / `vs2008/` / `branches/` å½å·¥ç¨å¥å£ï¼è§?[`docs/README.md`](../docs/README.md)ï¼?- **ä¸?*é»è®¤æå¼ `/W4` æ?sanitizers
- `use_fast_debug` å·²å£°æï¼MSVC ä»ç¨ `/Zi`
- Gen æ ¹æ¯ **`out/Debug` / `out/Release`**ï¼ä¸æ¯è£¸ `out/`ï¼ä¹ä¸æ¯ `out/Default`ï¼?- Harness æªå¾ / mark / loop æ¥åå?**`out/<config>/captures/`**ï¼agent æé¡¹æ¥å¿å?**`out/<config>/log/`** ä¸?**`out/scratch/`**ï¼å¯éæ¶æ¸çï¼?
æ°å¢ç¼è¯éé¡¹æ¶ï¼å?`config/` å?`config()`ï¼å `configs +=`ï¼ä¸è¦æ¹åä¸ª target çè£¸ flagsã?
---

**æåæ´æ°ï¼** 2026-10-15
