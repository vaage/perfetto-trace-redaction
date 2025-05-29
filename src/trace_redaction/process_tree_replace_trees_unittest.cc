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

#include "src/trace_redaction/process_tree_replace_trees.h"
#include <cstdint>

#include "perfetto/protozero/scattered_heap_buffer.h"
#include "src/base/test/status_matchers.h"
#include "src/trace_redaction/process_trees.h"
#include "src/trace_redaction/trace_redaction_framework.h"
#include "test/gtest_and_gmock.h"

namespace perfetto::trace_redaction {

namespace {
constexpr uint64_t kFirstProcessTreeTime = 1234;
}  // namespace

class ProcessTreeReplaceTreesTest : public testing::Test {
 protected:
  // Defaults to a valid configuration.
  struct Config {
    enum class AddToTracePacket {
      kNothing,
      kStandard,
      kOverflow,
    };

    enum class AddToContext {
      kNothing,
      kStandard,
    };

    uint64_t context_timestamp = kFirstProcessTreeTime;
    AddToContext context_processes = AddToContext::kStandard;
    AddToContext context_threads = AddToContext::kStandard;

    uint64_t trace_packet_timestamp = kFirstProcessTreeTime;
    AddToTracePacket trace_packet_processes = AddToTracePacket::kStandard;
    AddToTracePacket trace_packet_threads = AddToTracePacket::kStandard;
  };

  void BeginTest(const Config& config) {
    PopulateContext(config);

    protozero::HeapBuffered<protos::pbzero::TracePacket> message;
    message->set_timestamp(kFirstProcessTreeTime);

    // We can only call `message->set_process_tree()` once. Calling it more than
    // once will override the previous call.
    if (config.trace_packet_processes != Config::AddToTracePacket::kNothing ||
        config.trace_packet_threads != Config::AddToTracePacket::kNothing) {
      PopulateTracePacket(config, message->set_process_tree());
    }

    message_ = message.SerializeAsString();
  }

  std::vector<ProcessTrees::Process> CollectProcessesFromContext() const {
    const auto& tree = context_.process_trees;

    auto span = tree.processes();

    std::vector<ProcessTrees::Process> processes;

    for (size_t i = 0; i < span.length; ++i) {
      const auto process = span.data[i];

      if (context_.process_trees_mask.Has(process.timestamp, process.pid)) {
        processes.push_back(process);
      }
    }

    return processes;
  }

  std::vector<ProcessTrees::Thread> CollectThreadsFromContext() const {
    const auto& tree = context_.process_trees;

    auto span = tree.threads();

    std::vector<ProcessTrees::Thread> threads;

    for (size_t i = 0; i < span.length; ++i) {
      const auto thread = span.data[i];

      if (context_.process_trees_mask.Has(thread.timestamp, thread.tid)) {
        threads.push_back(thread);
      }
    }
    return threads;
  }

  base::StatusOr<std::vector<ProcessTrees::Process>>
  CollectProcessesFromTracePacket() const {
    protos::pbzero::TracePacket::Decoder decoder(message_);

    if (!decoder.has_process_tree()) {
      return base::ErrStatus("No process tree found in track packet.");
    }

    protos::pbzero::ProcessTree::Decoder process_tree_decoder(
        decoder.process_tree());

    if (!process_tree_decoder.has_processes()) {
      return base::ErrStatus("No processes found in process tree.");
    }

    std::vector<ProcessTrees::Process> processes;

    for (auto it = process_tree_decoder.processes(); it; ++it) {
      ProcessTrees::Process p;
      p.Initialize(kFirstProcessTreeTime, *it);

      processes.push_back(p);
    }

    return base::StatusOr<std::vector<ProcessTrees::Process>>(processes);
  }

  base::StatusOr<std::vector<ProcessTrees::Thread>>
  CollectThreadsFromTracePacket() const {
    protos::pbzero::TracePacket::Decoder decoder(message_);

    if (!decoder.has_process_tree()) {
      return base::ErrStatus("No process tree found in track packet.");
    }

    protos::pbzero::ProcessTree::Decoder process_tree_decoder(
        decoder.process_tree());

    if (!process_tree_decoder.has_threads()) {
      return base::ErrStatus("No threads found in process tree.");
    }

    std::vector<ProcessTrees::Thread> threads;

    for (auto it = process_tree_decoder.threads(); it; ++it) {
      ProcessTrees::Thread t;
      t.Initialize(kFirstProcessTreeTime, *it);

      threads.push_back(t);
    }

    return base::StatusOr<std::vector<ProcessTrees::Thread>>(threads);
  }

  Context context_;
  ProcessTreeReplaceTrees primitive_;
  std::string message_;

 private:
  void PopulateContext(const Config& config) {
    std::vector<ProcessTrees::Process> processes;
    std::vector<ProcessTrees::Thread> threads;

    switch (config.context_processes) {
      case Config::AddToContext::kNothing:
        break;
      case Config::AddToContext::kStandard:
        processes.insert(processes.end(), processes_.begin(), processes_.end());
        break;
    }

    switch (config.context_threads) {
      case Config::AddToContext::kNothing:
        break;
      case Config::AddToContext::kStandard:
        threads.insert(threads.end(), threads_.begin(), threads_.end());
        break;
    }

    for (auto& p : processes) {
      p.timestamp = config.context_timestamp;
      context_.process_trees_mask.Set(p.timestamp, p.pid);
    }

    context_.process_trees.Insert(processes.data(), processes.size());

    for (auto& t : threads) {
      t.timestamp = config.context_timestamp;
      context_.process_trees_mask.Set(t.timestamp, t.tid);
    }

    context_.process_trees.Insert(threads.data(), threads.size());
  }

  void PopulateTracePacket(
      const Config& config,
      perfetto::protos::pbzero::ProcessTree* process_tree) {
    std::vector<ProcessTrees::Process> processes;

    switch (config.trace_packet_processes) {
      case Config::AddToTracePacket::kNothing:
        break;

      case Config::AddToTracePacket::kStandard:
        processes.insert(processes.end(), processes_.begin(), processes_.end());
        break;

      case Config::AddToTracePacket::kOverflow:
        processes.insert(processes.end(), processes_.begin(), processes_.end());
        processes.insert(processes.end(), overflow_processes_.begin(),
                         overflow_processes_.end());
        break;
    }

    // Do not use a reference, we want to edit the value.
    for (auto p : processes) {
      p.timestamp = config.trace_packet_timestamp;
      p.WriteTo(*process_tree->add_processes());
    }

    std::vector<ProcessTrees::Thread> threads;

    switch (config.trace_packet_threads) {
      case Config::AddToTracePacket::kNothing:
        break;

      case Config::AddToTracePacket::kStandard:
        threads.insert(threads.end(), threads_.begin(), threads_.end());
        break;

      case Config::AddToTracePacket::kOverflow:
        threads.insert(threads.end(), threads_.begin(), threads_.end());
        threads.insert(threads.end(), overflow_threads_.begin(),
                       overflow_threads_.end());
        break;
    }

    // Do not use a reference, we want to edit the value.
    for (auto t : threads) {
      t.timestamp = config.trace_packet_timestamp;
      t.WriteTo(*process_tree->add_threads());
    }
  }

  const std::vector<ProcessTrees::Process> processes_ = {
      ProcessTrees::Process(kFirstProcessTreeTime, 0, -1, -1),
      ProcessTrees::Process(kFirstProcessTreeTime, 1, -1, -1),
  };

  const std::vector<ProcessTrees::Process> overflow_processes_ = {
      ProcessTrees::Process(kFirstProcessTreeTime, 2, -1, -1),
      ProcessTrees::Process(kFirstProcessTreeTime, 3, -1, -1),
  };

  const std::vector<ProcessTrees::Thread> threads_ = {
      ProcessTrees::Thread(kFirstProcessTreeTime, 10, -1),
      ProcessTrees::Thread(kFirstProcessTreeTime, 11, -1),
  };

  const std::vector<ProcessTrees::Thread> overflow_threads_ = {
      ProcessTrees::Thread(kFirstProcessTreeTime, 12, -1),
      ProcessTrees::Thread(kFirstProcessTreeTime, 13, -1),
  };
};

TEST_F(ProcessTreeReplaceTreesTest,
       NoChangeWhenBothTreesContainTheSameProcesses) {
  Config config;
  BeginTest(config);

  ASSERT_OK(primitive_.Transform(context_, &message_));

  auto context_process_trees = CollectProcessesFromContext();

  ASSERT_OK_AND_ASSIGN(auto trace_packet_process_trees,
                       CollectProcessesFromTracePacket());

  ASSERT_EQ(context_process_trees.size(), trace_packet_process_trees.size());

  for (size_t i = 0; i < context_process_trees.size(); ++i) {
    const auto& context_process = context_process_trees[i];
    const auto& trace_packet_process = trace_packet_process_trees[i];

    ASSERT_EQ(context_process.pid, trace_packet_process.pid)
        << "Processes do not match at index " << i;
  }
}

TEST_F(ProcessTreeReplaceTreesTest,
       NoChangeWhenBothTreesContainTheSameThreads) {
  Config config;
  BeginTest(config);

  ASSERT_OK(primitive_.Transform(context_, &message_));

  auto from_context = CollectThreadsFromContext();

  ASSERT_OK_AND_ASSIGN(auto from_trace_packet, CollectThreadsFromTracePacket());

  ASSERT_EQ(from_context.size(), from_trace_packet.size());

  for (size_t i = 0; i < from_context.size(); ++i) {
    const auto& context_thread = from_context[i];
    const auto& trace_packet_thread = from_trace_packet[i];

    ASSERT_EQ(context_thread.tid, trace_packet_thread.tid)
        << "Threads do not match at index " << i;
  }
}

// Has threads but no processes.
TEST_F(ProcessTreeReplaceTreesTest, FailsWhenContextHasNoProcesses) {
  Config config;
  config.context_processes = Config::AddToContext::kNothing;
  BeginTest(config);

  auto result = primitive_.Transform(context_, &message_);
  ASSERT_FALSE(result.ok()) << result.c_message();
}

// Has processes but no threads.
TEST_F(ProcessTreeReplaceTreesTest, FailsWhenContextHasNoThreads) {
  Config config;
  config.context_threads = Config::AddToContext::kNothing;
  BeginTest(config);

  auto result = primitive_.Transform(context_, &message_);
  ASSERT_FALSE(result.ok()) << result.c_message();
}

// When context has no processes, transform fails (see
// FailsWhenContextHasNoProcesses). However, if there are processes, but no
// processes for time T, then a process tree at time T should be cleared.
TEST_F(ProcessTreeReplaceTreesTest, ClearsProcessTreeWhenThereAreNoProcesses) {
  Config config;
  config.context_timestamp = kFirstProcessTreeTime - 1;
  BeginTest(config);

  ASSERT_OK(primitive_.Transform(context_, &message_));

  protos::pbzero::TracePacket::Decoder decoder(message_);
  ASSERT_FALSE(decoder.has_process_tree())
      << "No process tree found in track packet.";
}

// When context has no threads, transform fails (see
// FailsWhenContextHasNoThreads). However, if there are treads, but no threads
// for time T, then a process tree at time T should be cleared.
TEST_F(ProcessTreeReplaceTreesTest, ClearsProcessTreeWhenThereAreNoThreads) {
  Config config;
  config.context_timestamp = kFirstProcessTreeTime - 1;
  BeginTest(config);

  ASSERT_OK(primitive_.Transform(context_, &message_));

  protos::pbzero::TracePacket::Decoder decoder(message_);
  ASSERT_FALSE(decoder.has_process_tree())
      << "No process tree found in track packet.";
}

TEST_F(ProcessTreeReplaceTreesTest, CanReplaceProcessesWithASubset) {
  // The test has been configured to have more threads in the trace packet than
  // in the context. This means the threads in the trace packet will be reduced.
  Config config;
  config.trace_packet_threads = Config::AddToTracePacket::kOverflow;
  BeginTest(config);

  // Make sure the trace packet has more threads than the context.
  {
    auto from_context = CollectProcessesFromContext();

    ASSERT_OK_AND_ASSIGN(auto from_trace_packet,
                         CollectThreadsFromTracePacket());
    ASSERT_GT(from_trace_packet.size(), from_context.size());
  }

  ASSERT_OK(primitive_.Transform(context_, &message_));

  // Make sure the trace packet has the same number of threads as the context.
  {
    auto from_context = CollectProcessesFromContext();

    ASSERT_OK_AND_ASSIGN(auto from_trace_packet,
                         CollectProcessesFromTracePacket());
    ASSERT_EQ(from_trace_packet.size(), from_context.size());

    for (size_t i = 0; i < from_context.size(); ++i) {
      const auto& ctx_thread = from_context[i];
      const auto& tp_thread = from_trace_packet[i];

      ASSERT_EQ(ctx_thread.pid, tp_thread.pid)
          << "Threads do not match at index " << i;
    }
  }
}

TEST_F(ProcessTreeReplaceTreesTest, CanReplaceThreadsWithASubset) {
  // The test has been configured to have more threads in the trace packet than
  // in the context. This means the threads in the trace packet will be reduced.
  Config config;
  config.trace_packet_threads = Config::AddToTracePacket::kOverflow;
  BeginTest(config);

  // Make sure the trace packet has more threads than the context.
  {
    auto from_context = CollectThreadsFromContext();

    ASSERT_OK_AND_ASSIGN(auto from_trace_packet,
                         CollectThreadsFromTracePacket());
    ASSERT_GT(from_trace_packet.size(), from_context.size());
  }

  ASSERT_OK(primitive_.Transform(context_, &message_));

  // Make sure the trace packet has the same number of threads as the context.
  {
    auto from_context = CollectThreadsFromContext();

    ASSERT_OK_AND_ASSIGN(auto from_trace_packet,
                         CollectThreadsFromTracePacket());
    ASSERT_EQ(from_trace_packet.size(), from_context.size());

    for (size_t i = 0; i < from_context.size(); ++i) {
      const auto& ctx_thread = from_context[i];
      const auto& tp_thread = from_trace_packet[i];

      ASSERT_EQ(ctx_thread.tid, tp_thread.tid)
          << "Threads do not match at index " << i;
    }
  }
}

}  // namespace perfetto::trace_redaction
