/*
 * Copyright (C) 2025 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "src/trace_redaction/process_tree_create_mask.h"

#include "perfetto/base/status.h"
#include "src/trace_redaction/trace_redaction_framework.h"

namespace perfetto::trace_redaction {

base::Status ProcessTreeCreateMask::Build(Context* context) const {
  PERFETTO_DCHECK(filter_);

  if (!context->process_trees.processes().length) {
    return base::ErrStatus(
        "ProcessTreeCreateMask: process_trees has not been initialized, no "
        "processes were found.");
  }

  if (!context->process_trees.threads().length) {
    return base::ErrStatus(
        "ProcessTreeCreateMask: process_trees has not been initialized, no "
        "threads were found.");
  }

  auto processes = context->process_trees.processes();
  auto threads = context->process_trees.threads();

  auto& mask = context->process_trees_mask;

  for (size_t i = 0; i < processes.length; ++i) {
    const auto& process = processes.data[i];

    if (filter_->Includes(*context, process.timestamp, process.pid)) {
      mask.Set(process.timestamp, process.pid);
    }
  }

  for (size_t i = 0; i < threads.length; ++i) {
    const auto& thread = threads.data[i];

    if (filter_->Includes(*context, thread.timestamp, thread.tid)) {
      mask.Set(thread.timestamp, thread.tid);
    }
  }

  return base::OkStatus();
}

}  // namespace perfetto::trace_redaction
