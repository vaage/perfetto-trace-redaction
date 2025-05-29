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

#include "src/trace_redaction/process_trees.h"

#include <algorithm>
#include <cstdint>
#include <vector>

#include "protos/perfetto/trace/ps/process_tree.pbzero.h"

namespace perfetto::trace_redaction {
namespace {

// Resizes and appends elements to a vector.
template <typename T>
void Append(const T* data, size_t length, std::vector<T>& entries) {
  size_t old_size = entries.size();
  size_t new_size = entries.size() + length;

  entries.resize(new_size);

  for (size_t i = 0; i < length; ++i) {
    entries[old_size + i] = data[i];
  }
}

bool CompareProcessByTimestamp(const ProcessTrees::Process& l,
                               const ProcessTrees::Process& r) {
  return l.timestamp < r.timestamp;
}

bool CompareThreadByTimestamp(const ProcessTrees::Thread& l,
                              const ProcessTrees::Thread& r) {
  return l.timestamp < r.timestamp;
}

}  // namespace

base::Status ProcessTrees::Process::Initialize(uint64_t _timestamp,
                                               protozero::ConstBytes bytes) {
  protos::pbzero::ProcessTree::Process::Decoder decoder(bytes);

  if (!decoder.has_pid()) {
    return base::ErrStatus("Failed to populate process (missing pid field).");
  }

  if (!decoder.has_ppid()) {
    return base::ErrStatus("Failed to populate process (missing ppid field).");
  }

  if (!decoder.has_uid()) {
    return base::ErrStatus("Failed to populate process (missing uid field).");
  }

  timestamp = _timestamp;

  pid = decoder.pid();

  ppid = decoder.pid();

  uid = decoder.uid();

  for (auto it = decoder.cmdline(); it; ++it) {
    cmdline.push_back(it->as_std_string());
  }

  if (decoder.has_cmdline_is_comm()) {
    cmdline_is_comm = decoder.cmdline_is_comm();
  }

  for (auto it = decoder.nspid(); it; ++it) {
    nspid.push_back(it->as_int32());
  }

  if (decoder.has_process_start_from_boot()) {
    process_start_from_boot = decoder.process_start_from_boot();
  }

  if (decoder.has_is_kthread()) {
    is_kthread = decoder.is_kthread();
  }

  return base::OkStatus();
}

void ProcessTrees::Process::WriteTo(
    protos::pbzero::ProcessTree::Process& message) const {
  message.set_pid(pid);
  message.set_ppid(ppid);
  message.set_uid(uid);

  for (const auto& cmd : cmdline) {
    message.add_cmdline(cmd);
  }

  if (cmdline_is_comm.has_value()) {
    message.set_cmdline_is_comm(cmdline_is_comm.value());
  }

  for (auto nsp : nspid) {
    message.add_nspid(nsp);
  }

  if (process_start_from_boot.has_value()) {
    message.set_process_start_from_boot(process_start_from_boot.value());
  }

  if (is_kthread.has_value()) {
    message.set_is_kthread(is_kthread.value());
  }
}

base::Status ProcessTrees::Thread::Initialize(uint64_t _timestamp,
                                              protozero::ConstBytes bytes) {
  protos::pbzero::ProcessTree::Thread::Decoder decoder(bytes);

  if (!decoder.has_tid()) {
    return base::ErrStatus("Failed to populate thread (missing tid field).");
  }

  if (!decoder.has_tgid()) {
    return base::ErrStatus("Failed to populate thread (missing tgid field).");
  }

  timestamp = _timestamp;

  tid = decoder.tid();

  tgid = decoder.tgid();

  name = decoder.name().ToStdString();

  for (auto it = decoder.nstid(); it; ++it) {
    nstid.push_back(it->as_int32());
  }

  return base::OkStatus();
}

void ProcessTrees::Thread::WriteTo(
    protos::pbzero::ProcessTree::Thread& message) const {
  message.set_tid(tid);
  message.set_tgid(tgid);

  message.set_name(name);

  for (auto nst : nstid) {
    message.add_nstid(nst);
  }
}

void ProcessTrees::Insert(const ProcessTrees::Process* data, size_t length) {
  Append(data, length, processes_);
  std::sort(processes_.begin(), processes_.end(), CompareProcessByTimestamp);
}

void ProcessTrees::Insert(const ProcessTrees::Thread* data, size_t length) {
  Append(data, length, threads_);
  std::sort(threads_.begin(), threads_.end(), CompareThreadByTimestamp);
}

Span<ProcessTrees::Process> ProcessTrees::FetchProcesses(
    uint64_t trace_packet_timestamp) const {
  // We can only compare the same value. So we need to create a fake tree that
  // contains our timestamp.
  Process proxy;
  proxy.timestamp = trace_packet_timestamp;

  auto first = std::lower_bound(processes_.begin(), processes_.end(), proxy,
                                CompareProcessByTimestamp);
  auto last = first;

  // Copy all tree contents to the output. The trees are grouped by timestamp,
  // so once the timestamp no longer matches, we can stop searching.
  while (last != processes_.end() &&
         last->timestamp == trace_packet_timestamp) {
    ++last;
  }

  auto number_before = std::distance(processes_.begin(), first);
  auto number_after = std::distance(first, last);

  return Span<Process>(processes_.data() + number_before,
                       static_cast<size_t>(number_after));
}

Span<ProcessTrees::Thread> ProcessTrees::FetchThreads(
    uint64_t trace_packet_timestamp) const {
  // We can only compare the same value. So we need to create a fake tree that
  // contains our timestamp.
  Thread proxy;
  proxy.timestamp = trace_packet_timestamp;

  auto first = std::lower_bound(threads_.begin(), threads_.end(), proxy,
                                CompareThreadByTimestamp);
  auto last = first;

  // Copy all tree contents to the output. The trees are grouped by timestamp,
  // so once the timestamp no longer matches, we can stop searching.
  while (last != threads_.end() && last->timestamp == trace_packet_timestamp) {
    ++last;
  }

  auto number_before = std::distance(threads_.begin(), first);
  auto number_after = std::distance(first, last);

  return Span<Thread>(threads_.data() + number_before,
                      static_cast<size_t>(number_after));
}

}  // namespace perfetto::trace_redaction
