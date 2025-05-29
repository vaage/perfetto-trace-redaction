/*
 * Copyright (C) 2024 The Android Open Source Project
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

#include "src/trace_redaction/process_tree_replace_trees.h"

#include <string>

#include "perfetto/base/status.h"
#include "perfetto/protozero/field.h"
#include "perfetto/protozero/scattered_heap_buffer.h"
#include "src/trace_processor/util/status_macros.h"
#include "src/trace_redaction/proto_util.h"
#include "src/trace_redaction/trace_redaction_framework.h"

#include "protos/perfetto/trace/ps/process_tree.pbzero.h"
#include "protos/perfetto/trace/trace_packet.pbzero.h"

namespace perfetto::trace_redaction {
namespace {

base::Status PopulateProcessTree(
    const std::vector<ProcessTrees::Process>& processes,
    const std::vector<ProcessTrees::Thread>& threads,
    protozero::Field process_tree,
    protos::pbzero::ProcessTree* message) {
  protozero::ProtoDecoder decoder(process_tree.as_bytes());

  // Copy everything other than processes and threads. If processes and threads
  // should be copied, they will be copied via `processes` and `threads`.
  for (auto field = decoder.ReadField(); field.valid();
       field = decoder.ReadField()) {
    switch (field.id()) {
      case protos::pbzero::ProcessTree::kProcessesFieldNumber:
      case protos::pbzero::ProcessTree::kThreadsFieldNumber:
        break;
      default:
        proto_util::AppendField(field, message);
        break;
    }
  }

  for (const auto& process : processes) {
    process.WriteTo(*message->add_processes());
  }

  for (const auto& thread : threads) {
    thread.WriteTo(*message->add_threads());
  }

  return base::OkStatus();
}

std::vector<ProcessTrees::Thread> GetIncludedThreads(const Context& context,
                                                     uint64_t timestamp) {
  std::vector<ProcessTrees::Thread> included;

  auto threads = context.process_trees.FetchThreads(timestamp);

  for (size_t i = 0; i < threads.length; ++i) {
    auto thread = threads.data[i];

    if (context.process_trees_mask.Has(timestamp, thread.tid)) {
      included.push_back(thread);
    }
  }

  return included;
}

std::vector<ProcessTrees::Process> GetIncludedProcesses(const Context& context,
                                                        uint64_t timestamp) {
  std::vector<ProcessTrees::Process> included;

  auto processes = context.process_trees.FetchProcesses(timestamp);

  for (size_t i = 0; i < processes.length; ++i) {
    auto process = processes.data[i];

    if (context.process_trees_mask.Has(timestamp, process.pid)) {
      included.push_back(process);
    }
  }

  return included;
}

}  // namespace

base::Status ProcessTreeReplaceTrees::Transform(const Context& context,
                                                std::string* packet) const {
  PERFETTO_DCHECK(packet);

  if (context.process_trees_mask.entries().size() == 0) {
    return base::ErrStatus(
        "ProcessTreeReplaceTrees: process trees mask is empty.");
  }

  const auto& src = context.process_trees;

  if (src.processes().length == 0) {
    return base::ErrStatus(
        "ProcessTreeReplaceTrees: process trees has not been initialized (no "
        "processes).");
  }

  if (src.threads().length == 0) {
    return base::ErrStatus(
        "ProcessTreeReplaceTrees: process trees has not been initialized (no "
        "threads).");
  }

  protozero::ProtoDecoder decoder(*packet);

  auto timestamp_field =
      decoder.FindField(protos::pbzero::TracePacket::kTimestampFieldNumber);
  PERFETTO_DCHECK(timestamp_field.valid());
  auto timestamp = timestamp_field.as_uint64();

  protozero::HeapBuffered<protos::pbzero::TracePacket> message;

  for (auto field = decoder.ReadField(); field.valid();
       field = decoder.ReadField()) {
    if (field.id() != protos::pbzero::TracePacket::kProcessTreeFieldNumber) {
      proto_util::AppendField(field, message.get());
      continue;
    }

    auto included_processes = GetIncludedProcesses(context, timestamp);

    auto included_threads = GetIncludedThreads(context, timestamp);

    if (!included_processes.empty() || !included_threads.empty()) {
      RETURN_IF_ERROR(PopulateProcessTree(included_processes, included_threads,
                                          field, message->set_process_tree()));
    }
  }

  *packet = message.SerializeAsString();

  return base::OkStatus();
}

}  // namespace perfetto::trace_redaction
