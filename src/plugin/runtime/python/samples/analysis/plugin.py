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
        smartgis.ui.show_message_box(
            "info",
            "Sample Analysis stubs remain for education. "
            "Prefer builtin smartgis.traffic / smartgis.flood "
            "(native.cost_path / native.flood_fill).",
        )

    def _flood_fill(args_json):
        # Educational stub; real kernel is native.flood_fill / smartgis.flood.
        text = (args_json or "").strip()
        return text in ("", "{}")

    def _cost_path(args_json):
        # Educational stub; real kernel is native.cost_path / smartgis.traffic.
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
