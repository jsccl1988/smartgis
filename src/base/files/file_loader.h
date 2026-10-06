// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef BASE_FILES_FILE_LOADER_H_
#define BASE_FILES_FILE_LOADER_H_

#include <algorithm>
#include <cstddef>
#include <memory>
#include <string>
#include <vector>

#include "base/core/log.h"
#include "base/execution/pipeline/pipeline.h"
#include "base/files/file_mmap.h"
#include "base/trace/export/chrome_trace.h"
#include "base/trace/recorder/span_recorder.h"

namespace base {

// Three-stage FileMMap → parse → consume pipeline (mogu FileLoader).
// BlockHandler must expose: void* block; int block_size;
// Status on_block_complete(); Status on_record_complete();
// where Status is convertible from / to base::execution::Pipeline<>::Status
// (or use base::execution::detail::Status).
template <class BlockHandler>
struct FileLoader {
  using block_handler_t = BlockHandler;
  using pipeline_t = base::execution::Pipeline<block_handler_t>;

  std::unique_ptr<base::FileMMap> file_map;
  std::unique_ptr<pipeline_t> pipeline;

  bool enable_profile{false};
  std::size_t profile_span_reserve{0};
  std::unique_ptr<base::trace::SpanRecorder> profile_recorder;

  bool create(size_t parallel_num, const char* file_name,
              size_t block_size = 512 * 1024, bool warmup = true) {
    if (!file_name || block_size == 0) {
      LOGGING(LOG_ERROR,
              "FileLoader invalid parameters parallel_num[%zu] file_name[%p] "
              "block_size[%zu]",
              parallel_num, static_cast<const void*>(file_name), block_size);
      return false;
    }

    file_map = std::make_unique<base::FileMMap>(block_size);
    if (!file_map->open(file_name, warmup)) {
      return false;
    }

    using StageDescriptors = std::vector<typename pipeline_t::StageDescriptor>;
    using Status = typename pipeline_t::Status;
    StageDescriptors stage_descriptors(3);
    stage_descriptors[0].worker_num = 1;
    stage_descriptors[0].handler = [this](block_handler_t& handler) {
      return static_cast<Status>(
          file_map->next(&handler.block, &handler.block_size));
    };

    stage_descriptors[1].worker_num = (std::max)(size_t{1}, parallel_num);
    stage_descriptors[1].handler = [](block_handler_t& handler) {
      return static_cast<Status>(handler.on_block_complete());
    };

    stage_descriptors[2].worker_num = 1;
    stage_descriptors[2].handler = [](block_handler_t& handler) {
      return static_cast<Status>(handler.on_record_complete());
    };

    pipeline = std::make_unique<pipeline_t>(stage_descriptors);

    if (enable_profile) {
      profile_recorder = std::make_unique<base::trace::SpanRecorder>();
      const std::size_t n =
          profile_span_reserve == 0 ? 4096 : profile_span_reserve;
      profile_recorder->reserve(n);
      pipeline->set_stage_names({"produce", "parse", "consume"});
      pipeline->set_span_recorder(profile_recorder.get());
    } else {
      profile_recorder.reset();
    }

    return true;
  }

  bool run() {
    if (!pipeline) {
      return false;
    }
    pipeline->run();
    pipeline->wait();
    return true;
  }

  bool destroy() { return (!file_map || file_map->close()); }

  std::string export_chrome_trace() const {
    if (!profile_recorder) {
      return "{\"traceEvents\":[]}";
    }
    return base::trace::export_chrome_trace(*profile_recorder);
  }
};

}  // namespace base

#endif  // BASE_FILES_FILE_LOADER_H_
