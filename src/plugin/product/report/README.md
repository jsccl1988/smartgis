<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# smartgis.report

Product façade over `plugin::ReportBridge` (`plugin.report` capability). Chrome
owns WebView2 / fake-browser wiring; this package contributes `report.open` and
`report.post` so `--plugin-showcase=report` does not hardcode the dock in
CapabilityHost beyond opaque `run_processing`.
