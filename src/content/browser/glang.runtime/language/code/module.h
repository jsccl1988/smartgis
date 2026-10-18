// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

// Data IR for suite codegen. Slots live in SlotTable at eval, not in Module.
// $slot text is already split into StrPart by codegen.

#ifndef IL_RUNTIME_CODEGEN_CODE_MODULE_H_
#define IL_RUNTIME_CODEGEN_CODE_MODULE_H_

#include <cstdint>
#include <string>
#include <vector>

namespace content {
namespace code {

enum class ArgKind : uint8_t { kI32, kStr, kBool, kVk, kPath };

enum class Op : uint16_t {
  // session / horizon
  kDetachViews,
  kPump,
  kMark,
  kClearMarks,
  kSelectViewTab,
  kCatalogTab,
  kInspectorTab,
  kSuppressDialogs,
  kRequireHwnd,
  kWindow,
  kKey,
  kKeyEdge,
  kApplyUiTheme,
  kApplyScenarioPanels,
  kLayoutGate,
  kUiPresentCapture,
  kConsolePanBench,
  kExpectShellTree,
  kExpectLayoutBounds,
  kWireDebugAgent,
  kDebugExec,
  // map / document / view
  kOpenDocument,
  kDocClear,
  kFitExtent,
  kExportBmp,
  kApplyStyleFile,
  kCreateLayer,
  kRemoveLayer,
  kSetLayerVisible,
  kSelectLayer,
  kUpdateFeatureField,
  kWriteDocument,
  kClearSelection,
  kStopMapTimers,
  kResumeMapTimers,
  // Host-owned slots. Ordinals stay put so later ops do not move.
  // Script names and eval are registered by the host, not defined here.
  kHostSlotInvalidate,
  kRunTool,
  kActivateTool,
  kWaitMapReady,
  kHostSlotViewport,
  kEditToolSessionReady,
  kHostSlotBrowse,
  kHostSlotStill,
  kHostSlotBench,
  kHostSlotFit,
  kHostSlotCamera,
  kExpectTool,
  kExpectGeom,
  kExpectWheelCursor,
  kHostSlotVisible,
  kHostSlotOrbit,
  kHostSlotHwnd,
  // gesture
  kClick,
  kDrag,
  kWheel,
  kStroke,
  kPanBurst,
  kWheelBurst,
  // process / language
  kRunProcessing,
  kRunPluginCommand,
  kResolveData,
  kCapturePath,
  kSidecarPath,
  kRequireVar,
  kRequirePlugins,
  kAnalysisSetFrame,
  kAnalysisExportFrames,
  kOpenReport,
  kPostToReport,
  // Geo / Relation / Map. Geometry text is WKT in the string pool.
  // Append only. Do not renumber the enumerators above.
  kBuffer,
  kIntersection,
  kUnion,
  kDifference,
  kSymDifference,
  kCentroid,
  kEnvelope,
  kConvexHull,
  kBoundary,
  kSimplify,
  kTransform,
  kIntersects,
  kContains,
  kWithin,
  kDisjoint,
  kEquals,
  kTouches,
  kCrosses,
  kOverlaps,
  kGeomDistance,
  kGeomArea,
  kGeomLength,
  kIsEmpty,
  kScanFeatures,
  kFilterFeatures,
  kReadFeatureField,
  kProjectFeatures,
  kListLayers,
  kFeatureCount,
  kMapExtent,
  kPresentMode,
  kSetPresentMode,
  kStyleHasDocument,
  kStyleClear,
  kStyleSummary,
  kListAnalysisOps,
  kCount
};

// True for slots whose descriptor is supplied by the host verb table.
// A language-only link leaves them empty; that is not a missing builtin.
inline bool is_host_verb_slot(Op op) {
  switch (op) {
    case Op::kHostSlotInvalidate:
    case Op::kHostSlotViewport:
    case Op::kHostSlotBrowse:
    case Op::kHostSlotStill:
    case Op::kHostSlotBench:
    case Op::kHostSlotFit:
    case Op::kHostSlotCamera:
    case Op::kHostSlotVisible:
    case Op::kHostSlotOrbit:
    case Op::kHostSlotHwnd:
      return true;
    default:
      return false;
  }
}

// Source position copied from the script statement.
struct Loc {
  uint32_t line = 1;
  uint32_t column = 1;
};

// One script parameter. Operands of an Inst are stored in this array's order,
// not in script positional order. `positional` is the script index (-1 named-only).
struct Param {
  const char* name;
  int positional;
  ArgKind kind;
  bool required;
  int32_t default_i32;
  const char* default_str;  // nullable
};

// One piece of a $-split string. is_slot indexes SlotTable; otherwise strings.
struct StrPart {
  bool is_slot = false;
  uint32_t index = 0;
};

// Immediate or string/slot reference.
// aux == 0: index is a string-pool index (kStr / kPath / kVk).
// aux > 0: index is the first StrPart and aux is the part count.
// Path var=: kind == kPath, aux == 0, i32 < 0, index is a slot index.
// crs_epsg: 0 means unknown. The buffer verifier reads it on geometry operands.
// Geometry payload is WKT (or WKB hex) in the string pool: kind kStr / kPath,
// index selects `Module::strings`. There is no side table of geometry handles
// and no GEOS / OGR type in this header. coordinateDimension stays on the
// value produced at eval, not on a second IR type.
struct Operand {
  ArgKind kind = ArgKind::kI32;
  int32_t i32 = 0;
  uint32_t index = 0;
  uint16_t aux = 0;
  int32_t crs_epsg = 0;
};

// One straight-line instruction. result_slot is -1 when the verb has no as_.
struct Inst {
  Op op = Op::kMark;
  Loc loc;
  uint32_t operand_begin = 0;
  uint16_t operand_count = 0;
  int16_t result_slot = -1;
};

// Immutable program. Runtime slot values are not stored here.
struct Module {
  std::vector<std::string> strings;
  std::vector<std::string> slot_names;
  std::vector<StrPart> str_parts;
  std::vector<Operand> operands;
  std::vector<Inst> insts;
};

// Parallel to Module::slot_names. Eval writes produces_slot results here.
struct SlotTable {
  std::vector<std::string> values;
};

// Expand a string, path, or vk operand. Numeric kinds yield an empty string.
inline std::string expand_operand(const Module& module,
                                  const Operand& operand,
                                  const SlotTable& slots) {
  if (operand.aux > 0) {
    std::string out;
    const std::size_t begin = operand.index;
    const std::size_t end = begin + static_cast<std::size_t>(operand.aux);
    for (std::size_t i = begin; i < end && i < module.str_parts.size(); ++i) {
      const StrPart& part = module.str_parts[i];
      if (part.is_slot) {
        if (part.index < slots.values.size()) {
          out += slots.values[part.index];
        }
        continue;
      }
      if (part.index < module.strings.size()) {
        out += module.strings[part.index];
      }
    }
    return out;
  }
  if (operand.i32 < 0 && operand.kind == ArgKind::kPath) {
    if (operand.index < slots.values.size()) {
      return slots.values[operand.index];
    }
    return {};
  }
  if (operand.kind == ArgKind::kI32 || operand.kind == ArgKind::kBool) {
    return {};
  }
  if (operand.index < module.strings.size()) {
    return module.strings[operand.index];
  }
  return {};
}

}  // namespace code
}  // namespace content

#endif  // IL_RUNTIME_CODEGEN_CODE_MODULE_H_
