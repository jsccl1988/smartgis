# Copyright (c) 2026 The Mogu Authors.
# All rights reserved.
# Add and remove one layer. The host keys remove_layer by id.

from steps.support import fail_kwargs


def add_layer(sg, name, geometry="point", fail_rc=None):
    sg.create_layer(name=name, geometry=geometry, **fail_kwargs(fail_rc))


def remove_layer(sg, layer_id, fail_rc=None):
    sg.remove_layer(id=layer_id, **fail_kwargs(fail_rc))
