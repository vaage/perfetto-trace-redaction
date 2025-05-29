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
#include <memory>

#include "src/base/test/status_matchers.h"
#include "src/trace_redaction/process_tree_add_synth_threads.h"
#include "src/trace_redaction/process_trees.h"
#include "src/trace_redaction/trace_redaction_framework.h"
#include "test/gtest_and_gmock.h"

namespace perfetto::trace_redaction {
namespace {
constexpr uint64_t kTimestamp = 0;

static constexpr std::array<int32_t, 1> kPids = {0};
static constexpr std::array<int32_t, 2> kTids = {10, 11};

// The first tid is the main thread, which becomes the pid.
static constexpr std::array<int32_t, 3> kSynthTids = {100, 110, 111};
}  // namespace

class ProcessTreeAddSyncThreadsTest : public testing::Test {
 protected:
  void AddProcesses(ProcessTrees& process_trees) {
    for (auto pid : kPids) {
      ProcessTrees::Process process(kTimestamp, pid, -1, -1);
      process_trees.Insert(&process, 1);
    }
  }

  void AddThreads(ProcessTrees& process_trees) {
    for (auto tid : kTids) {
      ProcessTrees::Thread threads(kTimestamp, tid, -1);
      process_trees.Insert(&threads, 1);
    }
  }

  Context context_;
  const ProcessTreeAddSynthThreads primitive_;
};

TEST_F(ProcessTreeAddSyncThreadsTest, SynthThreadsAreAddedToMask) {
  context_.synthetic_process =
      std::make_unique<SyntheticProcess>(kSynthTids.data(), kSynthTids.size());

  AddProcesses(context_.process_trees);
  AddThreads(context_.process_trees);

  ASSERT_EQ(kSynthTids.size(), 3u);

  ASSERT_FALSE(context_.process_trees_mask.Has(kTimestamp, kSynthTids[0]));
  ASSERT_FALSE(context_.process_trees_mask.Has(kTimestamp, kSynthTids[1]));
  ASSERT_FALSE(context_.process_trees_mask.Has(kTimestamp, kSynthTids[2]));

  ASSERT_OK(primitive_.Build(&context_));

  ASSERT_TRUE(context_.process_trees_mask.Has(kTimestamp, kSynthTids[0]));
  ASSERT_TRUE(context_.process_trees_mask.Has(kTimestamp, kSynthTids[1]));
  ASSERT_TRUE(context_.process_trees_mask.Has(kTimestamp, kSynthTids[2]));
}

}  // namespace perfetto::trace_redaction
