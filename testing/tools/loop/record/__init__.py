# Copyright (c) 2026 The Mogu Authors.
# All rights reserved.

"""Harness record package: HWND capture, OS hooks, IL compact/emit.

Layout (compose, do not re-inflate hwnd.py):
  win32.py    — ctypes user32/gdi32 bindings
  window.py   — find HWND / z-order / map client
  capture.py  — BitBlt/PrintWindow + present quality gates
  ffmpeg.py   — ffmpeg path + frame→mp4
  session.py  — HwndRecorder session
  hwnd.py     — stable re-export facade for callers
  os_hook.py / il_recorder.py / agent_events.py — input record path
  il_compact.py / il_emit.py — events → ops → .il text
"""
