# Copyright (c) 2026 The Mogu Authors.
# All rights reserved.
# Open, save, and close the current document. close_document is doc_clear.

from steps.support import fail_kwargs


def open_path(sg, path, fail_rc=None):
    sg.open_document(path=path, **fail_kwargs(fail_rc))


def save_document(sg, path, fail_rc=None):
    sg.write_document(path=path, **fail_kwargs(fail_rc))


def close_document(sg):
    sg.doc_clear()
