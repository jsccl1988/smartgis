# Copyright (c) 2026 The Mogu Authors.
# All rights reserved.

"""Sample kind=python analysis plugin: dialog + flood/path processing stubs."""

import smartgis


PLUGIN_ID = "smartgis.sample_analysis"
DIALOG_ID = "sample.analysis.dlg"


def start(host):
    def _open_dialog(_args):
        return bool(host.open_dialog(DIALOG_ID))

    def _dialog_factory():
        # Ceiling placeholder UI until a real Views dialog is contributed.
        smartgis.ui.show_message_box(
            "info",
            "Sample Analysis: flood_fill / cost_path are processing stubs "
            "(empty args succeed). Real kernels land as native.* later.",
        )

    def _flood_fill(args_json):
        # Ceiling placeholder until native.flood_fill exists (phased).
        text = (args_json or "").strip()
        return text in ("", "{}")

    def _cost_path(args_json):
        # Ceiling placeholder until native.cost_path exists (phased).
        text = (args_json or "").strip()
        return text in ("", "{}")

    host.contribute_dialog(PLUGIN_ID, DIALOG_ID, "Sample Analysis", _dialog_factory)
    host.contribute_command(
        PLUGIN_ID, "sample.analysis.open", "Sample Analysis", "tools", _open_dialog
    )
    host.contribute_processing(
        PLUGIN_ID, "sample.flood_fill", "Flood fill (stub)", _flood_fill
    )
    host.contribute_processing(
        PLUGIN_ID, "sample.cost_path", "Cost path (stub)", _cost_path
    )


def stop():
    pass
