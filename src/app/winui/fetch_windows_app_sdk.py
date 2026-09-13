# Copyright (c) 2026 The Mogu Authors.
# All rights reserved.
"""Download Microsoft.WindowsAppSDK + WIL into third_party/windows_app_sdk."""

from __future__ import print_function

import json
import os
import sys
import zipfile

try:
    from urllib.request import urlopen
except ImportError:
    from urllib2 import urlopen

NUGET_FLAT = "https://api.nuget.org/v3-flatcontainer"
DEFAULT_WASDK = "1.7.260224002"
DEFAULT_WIL = "1.0.250325.1"


def _repo_root():
    here = os.path.abspath(os.path.dirname(__file__))
    return os.path.abspath(os.path.join(here, "..", "..", ".."))


def _download(url, dest):
    print("GET", url)
    sys.stdout.flush()
    resp = urlopen(url, timeout=120)
    data = resp.read()
    if hasattr(resp, "close"):
        resp.close()
    parent = os.path.dirname(dest)
    if parent and not os.path.isdir(parent):
        os.makedirs(parent)
    with open(dest, "wb") as f:
        f.write(data)
    print("wrote", dest, "(%d bytes)" % len(data))
    return dest


def _extract_nupkg(nupkg, dest_dir):
    if os.path.isdir(dest_dir):
        return dest_dir
    os.makedirs(dest_dir)
    with zipfile.ZipFile(nupkg, "r") as zf:
        zf.extractall(dest_dir)
    print("extracted", nupkg, "->", dest_dir)
    return dest_dir


def _latest_stable(package):
    url = "%s/%s/index.json" % (NUGET_FLAT, package.lower())
    print("GET", url)
    sys.stdout.flush()
    raw = urlopen(url, timeout=60).read()
    payload = json.loads(raw.decode("utf-8"))
    stables = [v for v in payload.get("versions", []) if "-" not in v]
    if not stables:
        return None
    return stables[-1]


def _nupkg_url(package, version):
    name = package.lower()
    return "%s/%s/%s/%s.%s.nupkg" % (NUGET_FLAT, name, version, name, version)


def main():
    root = _repo_root()
    dest_root = os.path.join(root, "third_party", "windows_app_sdk")
    if not os.path.isdir(dest_root):
        os.makedirs(dest_root)

    # Pin 1.7.x (fat nupkg). 2.x on nuget.org is a meta-package without headers.
    wasdk_ver = DEFAULT_WASDK
    try:
        latest = _latest_stable("Microsoft.WindowsAppSDK")
        if latest and latest.startswith("1.7."):
            wasdk_ver = latest
    except Exception as exc:
        print("WARN: nuget index failed (%s); pinning %s" % (exc, DEFAULT_WASDK))

    wil_ver = DEFAULT_WIL
    try:
        latest_wil = _latest_stable("Microsoft.Windows.ImplementationLibrary")
        if latest_wil:
            wil_ver = latest_wil
    except Exception as exc:
        print("WARN: WIL index failed (%s); pinning %s" % (exc, DEFAULT_WIL))

    wasdk_dir = os.path.join(dest_root, "Microsoft.WindowsAppSDK." + wasdk_ver)
    wil_dir = os.path.join(dest_root,
                           "Microsoft.Windows.ImplementationLibrary." + wil_ver)

    wasdk_nupkg = os.path.join(dest_root,
                               "Microsoft.WindowsAppSDK.%s.nupkg" % wasdk_ver)
    wil_nupkg = os.path.join(
        dest_root, "Microsoft.Windows.ImplementationLibrary.%s.nupkg" % wil_ver)

    try:
        if not os.path.isfile(wasdk_nupkg):
            _download(_nupkg_url("Microsoft.WindowsAppSDK", wasdk_ver),
                      wasdk_nupkg)
        _extract_nupkg(wasdk_nupkg, wasdk_dir)

        if not os.path.isfile(wil_nupkg):
            _download(
                _nupkg_url("Microsoft.Windows.ImplementationLibrary", wil_ver),
                wil_nupkg)
        _extract_nupkg(wil_nupkg, wil_dir)
    except Exception as exc:
        print("ERROR: failed to obtain Windows App SDK: %s" % exc)
        print("Expected extract: %s" % wasdk_dir)
        print("Manual: download Microsoft.WindowsAppSDK %s from nuget.org "
              "and unzip into that folder." % wasdk_ver)
        return 2

    version_path = os.path.join(dest_root, "VERSION")
    with open(version_path, "w") as f:
        f.write("Microsoft.WindowsAppSDK=%s\n" % wasdk_ver)
        f.write("Microsoft.Windows.ImplementationLibrary=%s\n" % wil_ver)
        f.write("root=%s\n" % wasdk_dir.replace("\\", "/"))

    print("WASDK_VERSION=%s" % wasdk_ver)
    print("WASDK_ROOT=%s" % wasdk_dir)
    print("WIL_ROOT=%s" % wil_dir)
    return 0


if __name__ == "__main__":
    sys.exit(main())
