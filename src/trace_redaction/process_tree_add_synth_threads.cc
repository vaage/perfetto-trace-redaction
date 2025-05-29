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

#include "src/trace_redaction/process_tree_add_synth_threads.h"

#include "perfetto/base/status.h"
#include "src/trace_redaction/trace_redaction_framework.h"

namespace perfetto::trace_redaction {

base::Status ProcessTreeAddSynthThreads::Build(Context* context) const {
  auto& trees = context->process_trees;
  auto& mask = context->process_trees_mask;

  if (!context->synthetic_process) {
    return base::ErrStatus(
        "ProcessTreeAddSynthThreads: cannot create process tree entries for "
        "synthetic threads, no synth process founds.");
  }

  if (!trees.processes().length) {
    return base::ErrStatus(
        "ProcessTreeAddSynthThreads: cannot create process tree entries for "
        "synthetic threads, pre-existing processes found.");
  }

  if (!trees.threads().length) {
    return base::ErrStatus(
        "ProcessTreeAddSynthThreads: cannot create process tree entries for "
        "synthetic threads, pre-existing threads found.");
  }

  const auto& synthetic_process = *context->synthetic_process;
  const auto& tids = synthetic_process.tids();

  if (tids.empty()) {
    return base::ErrStatus(
        "ProcessTreeAddSynthThreads: no synth threads found in synth process.");
  }

  // The first time for processes and threads should be the same, but this is
  // safer.
  auto first_timestamp = std::min(trees.processes().data->timestamp,
                                  trees.threads().data->timestamp);

  // The main thread is what appears as the process in the Perfetto UI.
  ProcessTrees::Process process(first_timestamp, tids.front(),
                                synthetic_process.ppid(),
                                synthetic_process.uid());
  process.cmdline = {"Other-Processes"};

  trees.Insert(&process, 1);

  for (size_t it = 1; it < tids.size(); ++it) {
    ProcessTrees::Thread thread(first_timestamp, tids[it],
                                synthetic_process.ppid());

    // Synth threads are created one-per-CPU core because re-assigning work by
    // CPU ensures tasks don't overlap.
    thread.name = std::to_string(it);
    thread.name.insert(0, "cpu-");

    trees.Insert(&thread, 1);
  }

  for (auto tid : tids) {
    mask.Set(first_timestamp, tid);
  }

  return base::OkStatus();
}

}  // namespace perfetto::trace_redaction
