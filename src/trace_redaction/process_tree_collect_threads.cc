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

#include "src/trace_redaction/process_tree_collect_threads.h"

#include "perfetto/base/status.h"
#include "src/trace_processor/util/status_macros.h"
#include "src/trace_redaction/trace_redaction_framework.h"

#include "protos/perfetto/trace/ps/process_tree.pbzero.h"

namespace perfetto::trace_redaction {
base::Status ProcessTreeCollectThreads::Begin(Context* context) const {
  PERFETTO_DCHECK(context);

  const auto& dest = context->process_trees;

  if (dest.processes().length) {
    return base::ErrStatus(
        "ProcessTreeCollectThreads: process_trees_all_threads has already been "
        "initialized. Why are there two primitives trying to initialize it?");
  }

  if (dest.threads().length) {
    return base::ErrStatus(
        "ProcessTreeCollectThreads: process_trees_all_threads has already been "
        "initialized. Why are there two primitives trying to initialize it?");
  }

  return base::OkStatus();
}

base::Status ProcessTreeCollectThreads::Collect(
    const protos::pbzero::TracePacket::Decoder& decoder,
    Context* context) const {
  PERFETTO_DCHECK(context);

  if (!decoder.has_timestamp()) {
    return base::ErrStatus(
        "ProcessTreeCollectThreads: trace packet is missing a timestamp.");
  }

  if (!decoder.has_process_tree()) {
    return base::OkStatus();
  }

  auto& dest = context->process_trees;

  protos::pbzero::ProcessTree::Decoder process_tree(decoder.process_tree());

  auto timestamp = decoder.timestamp();

  for (auto process_it = process_tree.processes(); process_it; ++process_it) {
    ProcessTrees::Process process;
    RETURN_IF_ERROR(process.Initialize(timestamp, *process_it));
    dest.Insert(&process, 1);
  }

  for (auto thread_it = process_tree.threads(); thread_it; ++thread_it) {
    ProcessTrees::Thread thread;
    RETURN_IF_ERROR(thread.Initialize(timestamp, *thread_it));
    dest.Insert(&thread, 1);
  }

  return base::OkStatus();
}

}  // namespace perfetto::trace_redaction
