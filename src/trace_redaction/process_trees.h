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

#ifndef SRC_TRACE_REDACTION_PROCESS_TREES_H_
#define SRC_TRACE_REDACTION_PROCESS_TREES_H_

#include <cstdint>
#include <optional>
#include <vector>

#include "perfetto/base/flat_set.h"
#include "perfetto/base/status.h"
#include "perfetto/protozero/field.h"
#include "protos/perfetto/trace/ps/process_tree.pbzero.h"

namespace perfetto::trace_redaction {

template <typename T>
struct Span {
  const T* data;
  const size_t length;

  Span(const T* _data, size_t _length) : data(_data), length(_length) {}
};

class ProcessTrees {
 public:
  struct Process {
    Process() = default;

    Process(uint64_t _timestamp, int32_t _pid, int32_t _ppid, int32_t _uid)
        : timestamp(_timestamp), pid(_pid), ppid(_ppid), uid(_uid) {}

    uint64_t timestamp;

    // Returns true if this Process was successfuly populated using the encoded
    // ProcessTree::Process. Otherwise, returns false and `this` is left in an
    // undefined state.
    base::Status Initialize(uint64_t _timestamp, protozero::ConstBytes bytes);

    void WriteTo(protos::pbzero::ProcessTree::Process& message) const;

    // A process must have a process ID, parent process group ID, and package
    // ID. If it does not have these three IDs it'll be dropped by other
    // redaction operations.
    int32_t pid = 1;
    int32_t ppid = 2;
    int32_t uid;

    // The command line for the process, as per /proc/pid/cmdline, broken up as
    // individual items.
    std::vector<std::string> cmdline;

    // If true, the |cmdline| field was filled with the main thread's "comm"
    // field instead.
    std::optional<bool> cmdline_is_comm;

    std::vector<int32_t> nspid;

    std::optional<uint64_t> process_start_from_boot = 7;

    std::optional<bool> is_kthread = 8;
  };

  // A copy of the proto definition of ProcessTree:Thread in
  // protos/perfetto/trace/ps/process_tree.pbzero.h
  //
  // We create a copy, instead of using the proto itself to make it easier to
  // create new entries (syth threads) and make it easier to debug and test.
  struct Thread {
    Thread() = default;

    Thread(uint64_t _timestamp, int32_t _tid, int32_t _tgid)
        : timestamp(_timestamp), tid(_tid), tgid(_tgid) {}

    // Returns true if this Thread was successfuly populated using the encoded
    // ProcessTree::Thread. Otherwise, returns false and `this` is left in an
    // undefined state.
    base::Status Initialize(uint64_t _timestamp, protozero::ConstBytes bytes);

    void WriteTo(protos::pbzero::ProcessTree::Thread& message) const;

    // The timestamp does not come from the thread; it comes from the trace
    // packet that contains the thread.
    uint64_t timestamp;

    // A thread must have a thread ID and thread group ID. If it does not have
    // a thread ID and/or thread group ID it'll be dropped by other redaction
    // operations.
    int32_t tid;
    int32_t tgid;

    // The name is optional. If a thread has no name, the name will be empty.
    std::string name;

    // This will always be empty for synthetic threads.
    std::vector<int32_t> nstid;
  };

  void Insert(const Process* processes, size_t length);

  void Insert(const Thread* threads, size_t length);

  Span<Process> FetchProcesses(uint64_t trace_packet_timestamp) const;

  Span<Thread> FetchThreads(uint64_t trace_packet_timestamp) const;

  Span<Process> processes() const {
    return Span<Process>(processes_.data(), processes_.size());
  }

  Span<Thread> threads() const {
    return Span<Thread>(threads_.data(), threads_.size());
  }

 private:
  // Store processes and threads in one vector (and not per-tree clusters) so
  // that they can be linearly iterated over.
  std::vector<Process> processes_;
  std::vector<Thread> threads_;
};

// Stores pids and timestamps. Each entry represents a pid within a process
// tree. Pids can be reused, so a single pid can/will appear more than one time
// in a mask. Pids are stored with a process tree identifier (trace packet
// timestamp).
class ProcessTreesMask {
 public:
  typedef std::pair<uint64_t, int32_t> Entry;

  void Set(uint64_t timestamp, int32_t pid) {
    auto entry = std::make_pair(timestamp, pid);

    included_.insert(entry);
    entries_.push_back(entry);
  }

  bool Has(uint64_t timestamp, int32_t pid) const {
    return included_.find(std::make_pair(timestamp, pid)) != included_.end();
  }

  const std::vector<Entry>& entries() const { return entries_; }

 private:
  base::FlatSet<Entry> included_;
  std::vector<Entry> entries_;
};

}  // namespace perfetto::trace_redaction

#endif  // SRC_TRACE_REDACTION_PROCESS_TREES_H_
