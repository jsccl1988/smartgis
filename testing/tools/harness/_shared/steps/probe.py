# Copyright (c) 2026 The Mogu Authors.
# All rights reserved.
# Feature-count assertions shared by edit, layer, and processing steps.


def require_count(sg, minimum, message):
    if int(sg.feature_count()) < minimum:
        raise RuntimeError(message)


def require_grew(sg, before, message):
    if int(sg.feature_count()) <= before:
        raise RuntimeError(message)
