<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# CEF Binary Distribution pin

- Dist: `cef_binary_149.0.4+g2f1bfd8+chromium-149.0.7827.156_windows64`
- CDN: `https://cef-builds.spotifycdn.com/` (percent-encode `+` as `%2B` in the URL)
- Unpack to: `third_party/cef/binary/` (gitignored contents; this README is tracked)
- Toolchain: MSVC x64 (matches this repo’s `out/` product builds)
- Wrapper: build `libcef_dll_wrapper` with CMake `-DCEF_RUNTIME_LIBRARY_FLAG=/MD`, then copy
  Debug (`/MDd`) and Release (`/MD`) `.lib` next to `libcef.lib` under `binary/Debug/` and
  `binary/Release/`. Product `is_debug=true` links Debug.

## Required for `smt_has_cef=true`

- `binary/include/cef_version.h`
- `binary/Release/libcef.lib` (Debug twin under `binary/Debug/` when linking Debug)
- `binary/Release/libcef_dll_wrapper.lib` (build the wrapper once via the dist CMake /
  `libcef_dll` sources if the unpack does not ship a prebuilt `.lib`)
- Runtime next to `out/SmartGisCef.exe`: `libcef.dll`, `chrome_elf.dll`, `icudtl.dat`,
  `*.bin` / `*.pak`, `locales/`, …

## Layout after unpack

```text
third_party/cef/binary/
  include/
  Release/   (or Debug/)
  Resources/
  libcef_dll/
```

Not vendored: Chromium source tree. Opt-in only: `build.bat cef` sets `smt_build_cef=true`.
Default `ninja all` / `src_all` never builds CEF.

## Fetch sketch (manual)

```bat
REM Download the pin tarball from cef-builds.spotifycdn.com, then:
mkdir third_party\cef\binary
tar -xf cef_binary_149.0.4+g2f1bfd8+chromium-149.0.7827.156_windows64.tar.bz2 -C third_party\cef\binary --strip-components=1
```

Build `libcef_dll_wrapper` with the dist CMake (VS x64) and place
`libcef_dll_wrapper.lib` beside `libcef.lib` under `Release/` (or `Debug/`).
