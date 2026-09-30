<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# third_party

���� mogu / mgis��`manifest.json` �� pin Ψһ���ࣻ`tools/fetch.py` ��Դ�뵽
**`third_party/.src/`**��`tools/batch.py` װ�� **`third_party/.install`**��
GN ֻ���Ѹ� prefix��`gn/tp.gni` + `gn/BUILD.gn` ���棩��**����**��
`cmake()` �� ninja ���������

Gitea ��ַ�� mogu / mgis ��ͬ��`http://localhost:3000/ccl`��`manifest.json` ��
`gitea_mirror`����`git_url` һ��ָ��� org�����Ѵ����� **����** mgis �� URL +
rev���������ϴ�����δ���ֵİ��� `git_url_fallbacks`��GitHub����fetch ������
Gitea �ٻ��ˡ��ǼǼ� [GITEA_MIRROR.md](GITEA_MIRROR.md)��

## ���

```bat
REM �����˰�װȫ�� install_skip=false ����Debug + MSVC��
build.bat t

REM ����
build.bat t glog
py -3 third_party\tools\fetch.py --package glog
py -3 third_party\tools\install.py --manifest third_party\manifest.json --package glog --src-root third_party\.src --build-root third_party\.build\glog --install-prefix third_party\.install --stamp third_party\.build\glog\.gn_publish_stamp --unified-prefix --build-type Debug
```

`.src/<name>` ������������clone / zip / junction��ʱ fetch **����**������
`rmtree` �� junction Ŀ�ꡣ���� `third_party/<name>/CMakeLists.txt` ���ڣ�����
vendor��ʱ�� vendored skip���� `BUILD.gn` ��װĿ¼ **��** ��Դ������ǿ��������
`set MGIS_TP_FORCE_FETCH=1`��

Ĭ�� `build.bat`����Ʒͼ��**��**������Դ�롣��Ʒֻ���� `//third_party:<name>`��

## �ṹ

| Path | ˵�� |
| --- | --- |
| `manifest.json` | `incubator_third_party_manifest_v1`��GIS ��Ŀ���� mgis����ӱ��ְ��� |
| `tools/` | mogu `fetch.py` / `install.py` / `batch.py` / `deps.py` |
| `.src/` | **���п�Դ��**��gitignore����fetch ��¡���򱾵� junction �� mgis |
| `.src/_cache/` | ��ѡ���ػ��棨gitignore�������� MapLibre vendor tarball |
| `.build/<pkg>/` | ���� CMake ����gitignore�� |
| `.install/` | �ϲ���װǰ׺��gitignore����GN `third_party_install_prefix` |
| `gn/` | `tp.gni` + ���� `config`/`group` |
| `BUILD.gn` | `//third_party:<name>` �� `//third_party/gn:<name>` |
| `CxImage/` `antlr4/` `ed25519/` `khronos_gl/` `flycube/` | **�� GN**��`install_skip`����Դ���� `.src/<name>` |
| `cximage_pub/` | ���� MBCS ����ͷ��overlay������ Gitea�� |
| `windows_app_sdk/` | ���� WinAppSDK / WebView2 ��������ϴ������վ� UI�� |
| `python/` | ���� embeddable CPython�����ϴ��� |
| `bcg/` | **��Ҫ**���ػ���� BCG |
| `GITEA_MIRROR.md` | ���� Gitea ״̬ |

����������

```bat
mklink /J third_party\.install c:\Dev\src\gis\mgis\out\third_party
mklink /J third_party\.src\gdal c:\Dev\src\gis\mgis\third_party\gdal
```

**ͳһ��װǰ׺��** GDAL��FlyCube Release `/MD` libs���Լ������������ã�MapLibre
Native �� install ����һ�ɽ� **`third_party/.install`**��`bin/` / `include/` /
`lib/` / `share/`������Ҫ��ά������ `gdal_sdk/`��`flycube_prebuilt/` ��
`out/third_party/maplibre` ��Ϊ�ڶ���װ����FlyCube Դ������ `.src/flycube`��
Release �� `FlyCube.lib` �ȿ��� `.install/lib` ���ɣ�Debug �Աൽ
`out/*/flycube`����

**������** `.install/lib` ֻ������ import lib����Ҫ�Ѳ�Ʒ `*_d.dll.lib` /
`base_d.dll.lib` �ȿ���ȥ��`concurrentqueue` ͷ�ļ�ֻ��
`.src/concurrentqueue/moodycamel/`���� GN `include_dirs` �� `.src`����`.src`
��Ŀ¼��Ҫ�� tarball������Ŀ¼���޹� symlink��`skia` �ȣ���

��Ҫ��Դ�� junction ���� `third_party/<name>/` ���㣨�ǻ�ͱ� GN ��Ŀ¼����

## �� mogu �Ĳ���

- Windows / MSVC��`install.py` �� VS generator���� mgis ������
- ������ Bazel
- GIS ���嵥�� mgis ��ͬ��Gitea URL + rev ԭ�����ã�
- �������� `khronos_gl` / `flycube` / `antlr4` / `eigen` / `ed25519` ��

`is_build_third_party` �����ã��������� `cmake()`��ȱ��ʱ�� `build.bat t`��

---

**�����£�** 2026-09-29
