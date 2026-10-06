// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef PLUGIN_REPORT_COMMANDS_H_
#define PLUGIN_REPORT_COMMANDS_H_

namespace content {
class PluginHost;
}

namespace plugin {

// Local HTML report dock via plugin.report capability (ReportBridge).
bool register_report(content::PluginHost* host);

}  // namespace plugin

#endif  // PLUGIN_REPORT_COMMANDS_H_
