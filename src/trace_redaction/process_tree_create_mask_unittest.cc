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

#include "src/base/test/status_matchers.h"
#include "src/trace_redaction/process_tree_create_mask.h"
#include "src/trace_redaction/process_trees.h"
#include "src/trace_redaction/trace_redaction_framework.h"
#include "test/gtest_and_gmock.h"

namespace perfetto::trace_redaction {
namespace {
constexpr uint64_t kFirstProcessTreeTime = 1234;
}  // namespace

class ProcessTreeCreateMaskTest : public testing::Test {
 protected:
  Context context_;
  ProcessTreeCreateMask primitive_;

  const std::array<ProcessTrees::Process, 2> processes_ = {
      ProcessTrees::Process(kFirstProcessTreeTime, 0, -1, -1),
      ProcessTrees::Process(kFirstProcessTreeTime, 1, -1, -1),
  };

  const std::array<ProcessTrees::Thread, 2> threads_ = {
      ProcessTrees::Thread(kFirstProcessTreeTime, 10, -1),
      ProcessTrees::Thread(kFirstProcessTreeTime, 11, -1),
  };
};

// Add a thread so that the processes-check will triggered.
TEST_F(ProcessTreeCreateMaskTest, RequiresProcessTreesToHaveProcesses) {
  primitive_.set_filter(std::make_unique<AllowAll>());

  ProcessTrees::Thread thread(kFirstProcessTreeTime, 10, 1);
  context_.process_trees.Insert(&thread, 1);

  auto result = primitive_.Build(&context_);
  ASSERT_FALSE(result.ok()) << result.c_message();
}

// Add a process so that the thread-check will triggered.
TEST_F(ProcessTreeCreateMaskTest, RequiresProcessTreesToHaveThreads) {
  primitive_.set_filter(std::make_unique<AllowAll>());

  ProcessTrees::Process process(kFirstProcessTreeTime, 10, 1, 1);
  context_.process_trees.Insert(&process, 1);

  auto result = primitive_.Build(&context_);
  ASSERT_FALSE(result.ok()) << result.c_message();
}

TEST_F(ProcessTreeCreateMaskTest, AddsProcessesToProcessTreesMask) {
  primitive_.set_filter(std::make_unique<AllowAll>());

  context_.process_trees.Insert(processes_.data(), processes_.size());
  context_.process_trees.Insert(threads_.data(), threads_.size());

  // Empty mask before running primitive.
  ASSERT_EQ(context_.process_trees_mask.entries().size(), 0u);

  ASSERT_OK(primitive_.Build(&context_));

  // Populated mask after running primitive.
  ASSERT_EQ(context_.process_trees_mask.entries().size(),
            processes_.size() + threads_.size());
}

}  // namespace perfetto::trace_redaction
