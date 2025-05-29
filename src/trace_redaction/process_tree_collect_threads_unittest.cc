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

#include <cstdint>

#include "perfetto/protozero/scattered_heap_buffer.h"
#include "src/base/test/status_matchers.h"
#include "src/trace_redaction/process_tree_collect_threads.h"
#include "src/trace_redaction/process_trees.h"
#include "src/trace_redaction/trace_redaction_framework.h"
#include "test/gtest_and_gmock.h"

namespace perfetto::trace_redaction {
namespace {
constexpr uint64_t kFirstProcessTreeTime = 1234;
}  // namespace

class ProcessTreeCollectThreadsTest : public testing::Test {
 protected:
  // Defaults to a valid configuration.
  struct Config {
    // When true, adds processes to the trace packet.
    bool add_processes_to_trace_packet = false;

    // When true, adds threads to the trace packet.
    bool add_threads_to_trace_packet = false;

    // When true, adds processes to the context before invoking the primitive.
    bool add_processes_to_context = false;

    // When true, adds threads to the context before invoking the primitive.
    bool add_threads_to_context = false;
  };

  // Configure the test using the configuration. This will set-up the tree with
  // the test's global processes and threads.
  void BeginTest(const Config& config) {
    protozero::HeapBuffered<protos::pbzero::TracePacket> message;
    message->set_timestamp(kFirstProcessTreeTime);

    // We can only call `message->set_process_tree()` once. Calling it more than
    // once will override the previous call.
    if (config.add_processes_to_trace_packet ||
        config.add_threads_to_trace_packet) {
      BeginTest(config, message->set_process_tree());
    }

    if (config.add_processes_to_context) {
      context_.process_trees.Insert(processes_.data(), processes_.size());
    }

    if (config.add_threads_to_context) {
      context_.process_trees.Insert(threads_.data(), threads_.size());
    }

    message_ = message.SerializeAsString();
  }

  Context context_;
  const ProcessTreeCollectThreads primitive_;
  std::string message_;

 private:
  void BeginTest(const Config& config,
                 perfetto::protos::pbzero::ProcessTree* process_tree) {
    if (config.add_processes_to_trace_packet) {
      for (const auto& process : processes_) {
        process.WriteTo(*process_tree->add_processes());
      }
    }

    if (config.add_threads_to_trace_packet) {
      for (const auto& thread : threads_) {
        thread.WriteTo(*process_tree->add_threads());
      }
    }
  }

  const std::vector<ProcessTrees::Process> processes_ = {
      ProcessTrees::Process(kFirstProcessTreeTime, 0, -1, -1),
  };

  const std::vector<ProcessTrees::Thread> threads_ = {
      ProcessTrees::Thread(kFirstProcessTreeTime, 10, -1),
      ProcessTrees::Thread(kFirstProcessTreeTime, 11, -1),
  };
};

TEST_F(ProcessTreeCollectThreadsTest, ProcessTreeShouldHaveNoProcesses) {
  Config config;
  config.add_processes_to_context = true;

  BeginTest(config);

  protos::pbzero::TracePacket::Decoder decoder(message_);

  auto result = primitive_.Begin(&context_);
  ASSERT_FALSE(result.ok()) << result.c_message();
}

TEST_F(ProcessTreeCollectThreadsTest, ProcessTreeShouldHaveNoThreads) {
  Config config;
  config.add_threads_to_context = true;

  BeginTest(config);

  protos::pbzero::TracePacket::Decoder decoder(message_);

  auto result = primitive_.Begin(&context_);
  ASSERT_FALSE(result.ok()) << result.c_message();
}

TEST_F(ProcessTreeCollectThreadsTest, WillCollectProcessesFromTracePacket) {
  Config config;
  config.add_processes_to_trace_packet = true;
  config.add_threads_to_trace_packet = true;

  BeginTest(config);

  protos::pbzero::TracePacket::Decoder decoder(message_);

  ASSERT_OK(primitive_.Begin(&context_));
  ASSERT_OK(primitive_.Collect(decoder, &context_));
  ASSERT_OK(primitive_.End(&context_));

  ASSERT_GT(context_.process_trees.processes().length, 0u);
}

TEST_F(ProcessTreeCollectThreadsTest, WillCollectThreadsFromTracePacket) {
  Config config;
  config.add_processes_to_trace_packet = true;
  config.add_threads_to_trace_packet = true;

  BeginTest(config);

  protos::pbzero::TracePacket::Decoder decoder(message_);

  ASSERT_OK(primitive_.Begin(&context_));
  ASSERT_OK(primitive_.Collect(decoder, &context_));
  ASSERT_OK(primitive_.End(&context_));

  ASSERT_GT(context_.process_trees.threads().length, 0u);
}

}  // namespace perfetto::trace_redaction
