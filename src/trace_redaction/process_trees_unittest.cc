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

#include "src/trace_redaction/process_trees.h"
#include "test/gtest_and_gmock.h"

namespace perfetto::trace_redaction {

namespace {
constexpr uint64_t kFirstProcessTreeTime = 1234;
}  // namespace

class ProcessTreesTest : public testing::Test {
 protected:
  ProcessTrees trees_;

  const std::vector<ProcessTrees::Process> processes_ = {
      ProcessTrees::Process(kFirstProcessTreeTime, 0, -1, -1),
      ProcessTrees::Process(kFirstProcessTreeTime, 1, -1, -1),
      ProcessTrees::Process(kFirstProcessTreeTime, 2, -1, -1),
  };

  const std::vector<ProcessTrees::Thread> threads_ = {
      ProcessTrees::Thread(kFirstProcessTreeTime, 10, -1),
      ProcessTrees::Thread(kFirstProcessTreeTime, 11, -1),
      ProcessTrees::Thread(kFirstProcessTreeTime, 12, -1),
  };
};

TEST_F(ProcessTreesTest, CanFetchProcessesAfterInsert) {
  trees_.Insert(processes_.data(), processes_.size());
  auto processes = trees_.FetchProcesses(kFirstProcessTreeTime);

  ASSERT_EQ(processes.length, processes_.size());
  ASSERT_EQ(processes.data[0].pid, processes_[0].pid);
  ASSERT_EQ(processes.data[1].pid, processes_[1].pid);
  ASSERT_EQ(processes.data[2].pid, processes_[2].pid);
}

TEST_F(ProcessTreesTest, CanFetchThreadsAfterInsert) {
  trees_.Insert(threads_.data(), threads_.size());
  auto threads = trees_.FetchThreads(kFirstProcessTreeTime);

  ASSERT_EQ(threads.length, threads_.size());
  ASSERT_EQ(threads.data[0].tid, threads_[0].tid);
  ASSERT_EQ(threads.data[1].tid, threads_[1].tid);
  ASSERT_EQ(threads.data[2].tid, threads_[2].tid);
}

TEST_F(ProcessTreesTest, NoProcessesForMissingTime) {
  trees_.Insert(processes_.data(), processes_.size());
  auto processes = trees_.FetchProcesses(kFirstProcessTreeTime + 1);

  ASSERT_EQ(processes.length, 0u);
}

TEST_F(ProcessTreesTest, NoThreadsForMissingTime) {
  trees_.Insert(threads_.data(), threads_.size());
  auto threads = trees_.FetchThreads(kFirstProcessTreeTime + 1);

  ASSERT_EQ(threads.length, 0u);
}

TEST_F(ProcessTreesTest, NoProcessesWhenEmpty) {
  auto processes = trees_.FetchProcesses(kFirstProcessTreeTime);
  ASSERT_EQ(processes.length, 0u);
}

TEST_F(ProcessTreesTest, NoThreadsWhenEmpty) {
  auto threads = trees_.FetchThreads(kFirstProcessTreeTime);
  ASSERT_EQ(threads.length, 0u);
}

TEST_F(ProcessTreesTest, ReturnsSubsetOfProcesses) {
  std::vector<ProcessTrees::Process> processes = {
      ProcessTrees::Process(kFirstProcessTreeTime, 0, -1, -1),
      ProcessTrees::Process(kFirstProcessTreeTime, 1, -1, -1),
      ProcessTrees::Process(kFirstProcessTreeTime, 2, -1, -1),
      ProcessTrees::Process(kFirstProcessTreeTime + 1, 3, -1, -1),
      ProcessTrees::Process(kFirstProcessTreeTime + 1, 4, -1, -1),
  };

  trees_.Insert(processes.data(), processes.size());

  {
    auto span = trees_.FetchProcesses(kFirstProcessTreeTime);
    ASSERT_EQ(span.length, 3u);
    ASSERT_EQ(span.data[0].pid, 0);
    ASSERT_EQ(span.data[1].pid, 1);
    ASSERT_EQ(span.data[2].pid, 2);
  }
  {
    auto span = trees_.FetchProcesses(kFirstProcessTreeTime + 1);
    ASSERT_EQ(span.length, 2u);
    ASSERT_EQ(span.data[0].pid, 3);
    ASSERT_EQ(span.data[1].pid, 4);
  }
}

TEST_F(ProcessTreesTest, ReturnsSubsetOfThreads) {
  std::vector<ProcessTrees::Thread> threads = {
      ProcessTrees::Thread(kFirstProcessTreeTime, 0, -1),
      ProcessTrees::Thread(kFirstProcessTreeTime, 1, -1),
      ProcessTrees::Thread(kFirstProcessTreeTime, 2, -1),
      ProcessTrees::Thread(kFirstProcessTreeTime + 1, 3, -1),
      ProcessTrees::Thread(kFirstProcessTreeTime + 1, 4, -1),
  };

  trees_.Insert(threads.data(), threads.size());

  {
    auto span = trees_.FetchThreads(kFirstProcessTreeTime);
    ASSERT_EQ(span.length, 3u);
    ASSERT_EQ(span.data[0].tid, 0);
    ASSERT_EQ(span.data[1].tid, 1);
    ASSERT_EQ(span.data[2].tid, 2);
  }
  {
    auto span = trees_.FetchThreads(kFirstProcessTreeTime + 1);
    ASSERT_EQ(span.length, 2u);
    ASSERT_EQ(span.data[0].tid, 3);
    ASSERT_EQ(span.data[1].tid, 4);
  }
}

}  // namespace perfetto::trace_redaction
