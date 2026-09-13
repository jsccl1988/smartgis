#!/usr/bin/env python3
# Copyright (c) 2026 The Mogu Authors.
# All rights reserved.
"""Unit tests for third_party/tools/fetch.py GitHub fetch helpers."""

from __future__ import annotations

import os
import unittest
from pathlib import Path
from unittest import mock

import fetch


class GithubUrlMirrorTest(unittest.TestCase):
    def test_prefixes_github_https_when_mirror_set(self) -> None:
        url = "https://github.com/maplibre/maplibre-native.git"
        with mock.patch.dict(os.environ, {"MOGU_GITHUB_MIRROR": "https://ghproxy.net"}):
            self.assertEqual(
                fetch._github_url_with_optional_mirror(url),
                "https://ghproxy.net/https://github.com/maplibre/maplibre-native.git",
            )

    def test_leaves_non_github_url_unchanged(self) -> None:
        url = "https://gitlab.com/libeigen/eigen.git"
        with mock.patch.dict(os.environ, {"MOGU_GITHUB_MIRROR": "https://ghproxy.net"}):
            self.assertEqual(fetch._github_url_with_optional_mirror(url), url)

    def test_no_mirror_returns_original(self) -> None:
        url = "https://github.com/maplibre/maplibre-native.git"
        env = os.environ.copy()
        env.pop("MOGU_GITHUB_MIRROR", None)
        with mock.patch.dict(os.environ, env, clear=True):
            self.assertEqual(fetch._github_url_with_optional_mirror(url), url)


class GitHttpConfigTest(unittest.TestCase):
    def test_forces_http11_to_avoid_gnutls_http2_reset(self) -> None:
        args = fetch._git_http_config_args()
        self.assertIn("http.version=HTTP/1.1", args)
        self.assertIn("http.postBuffer=524288000", args)

    def test_run_git_passes_http11_config(self) -> None:
        with mock.patch("fetch.subprocess.run") as run:
            run.return_value = mock.Mock(returncode=0)
            fetch._run_git(["clone", "https://example.com/r.git", "/tmp/x"])
            args = run.call_args[0][0]
            self.assertEqual(args[0], "git")
            self.assertIn("http.version=HTTP/1.1", args)
            self.assertIn("clone", args)


class GithubUrlCandidatesTest(unittest.TestCase):
    def test_direct_then_ghproxy_when_no_env_mirror(self) -> None:
        url = "https://github.com/maplibre/maplibre-native/archive/refs/tags/ios-v6.30.0.tar.gz"
        env = os.environ.copy()
        env.pop("MOGU_GITHUB_MIRROR", None)
        with mock.patch.dict(os.environ, env, clear=True):
            cands = fetch._github_url_candidates(url)
        self.assertEqual(cands[0], url)
        self.assertIn(
            "https://ghproxy.net/https://github.com/maplibre/maplibre-native/archive/refs/tags/ios-v6.30.0.tar.gz",
            cands,
        )

    def test_env_mirror_is_first_candidate(self) -> None:
        url = "https://github.com/maplibre/maplibre-native.git"
        with mock.patch.dict(os.environ, {"MOGU_GITHUB_MIRROR": "https://ghproxy.net"}):
            cands = fetch._github_url_candidates(url)
        self.assertEqual(
            cands[0],
            "https://ghproxy.net/https://github.com/maplibre/maplibre-native.git",
        )


class GithubArchiveUrlTest(unittest.TestCase):
    def test_tag_archive_url_for_github_git_repo(self) -> None:
        self.assertEqual(
            fetch._github_archive_url(
                "https://github.com/maplibre/maplibre-native.git",
                "ios-v6.30.0",
            ),
            "https://github.com/maplibre/maplibre-native/archive/refs/tags/ios-v6.30.0.tar.gz",
        )

    def test_none_for_non_github(self) -> None:
        self.assertIsNone(
            fetch._github_archive_url(
                "https://gitlab.com/libeigen/eigen.git",
                "3.4.0",
            )
        )


class ArchiveCompleteTest(unittest.TestCase):
    def test_truncated_gzip_is_incomplete(self) -> None:
        import gzip
        import tempfile

        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "ios-v6.30.0.tar.gz"
            with gzip.open(path, "wb") as gz:
                gz.write(b"not-a-full-tarball")
            truncated = path.read_bytes()[:8]
            path.write_bytes(truncated)
            self.assertFalse(fetch._archive_complete(path))

    def test_complete_gzip_is_ok(self) -> None:
        import gzip
        import tempfile

        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "ok.tar.gz"
            with gzip.open(path, "wb") as gz:
                gz.write(b"payload")
            self.assertTrue(fetch._archive_complete(path))


class GitCloneRetryTest(unittest.TestCase):
    def test_clone_retry_env_defaults(self) -> None:
        env = os.environ.copy()
        env.pop("MOGU_GIT_CLONE_MAX_ATTEMPTS", None)
        with mock.patch.dict(os.environ, env, clear=True):
            self.assertEqual(fetch._git_clone_max_attempts(), 5)

    def test_rmtree_if_exists_removes_partial_clone(self) -> None:
        import tempfile

        with tempfile.TemporaryDirectory() as tmp:
            dest = Path(tmp) / "maplibre-native"
            dest.mkdir()
            (dest / "partial").write_text("x", encoding="utf-8")
            fetch._rmtree_if_exists(dest)
            self.assertFalse(dest.exists())


if __name__ == "__main__":
    unittest.main()
