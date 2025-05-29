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
#include "src/trace_redaction/process_tree_add_synth_threads.h"
#include "src/trace_redaction/process_tree_collect_threads.h"
#include "src/trace_redaction/process_tree_create_mask.h"
#include "src/trace_redaction/process_tree_replace_trees.h"
#include "src/trace_redaction/trace_redaction_framework.h"
#include "test/gtest_and_gmock.h"

namespace perfetto::trace_redaction {
namespace {

// Builders are stateful and are meant to make building trace packets easier.
// Builders are a one-time-use and should not be used after calling Build();
class Builder {
 public:
  explicit Builder(uint64_t timestamp) {
    message_->set_timestamp(timestamp);
    process_tree_ = message_->set_process_tree();
  }

  void AddPackage(int32_t uid) { uid_ = uid; }

  void AddProcess(int32_t ppid, int32_t pid) {
    PERFETTO_DCHECK(uid_ != -1);

    auto* process = process_tree_->add_processes();
    process->set_uid(uid_);
    process->set_ppid(ppid);
    process->set_pid(pid);

    pid_ = pid;
  }

  void AddThread(int32_t tid) {
    PERFETTO_DCHECK(pid_ != -1);

    auto* thread = process_tree_->add_threads();
    thread->set_tgid(pid_);
    thread->set_tid(tid);
  }

  std::string Build() { return message_.SerializeAsString(); }

 private:
  protozero::HeapBuffered<protos::pbzero::TracePacket> message_;
  protos::pbzero::ProcessTree* process_tree_ = nullptr;

  int32_t uid_ = -1;
  int32_t pid_ = -1;
};
}  // namespace

class ProcessTreeEndToEndTest : public testing::Test {
 protected:
  void SetUp() override {
    context_.package_uid = 1;

    context_.synthetic_process = std::make_unique<SyntheticProcess>(
        synth_threads.data(), synth_threads.size());

    // Simple start state, two processes. Only one of the two processes belong
    // to our target package. The first package must be 1 or greater. 0 is a
    // reserved uid; see the process thread timeline for more information.
    //
    //  > Track Packet 0
    //      > Package 1:
    //          > Process 0:    [ x ]
    //              > Thread 1  [ x ]
    //              > Thread 2  [ x ]
    //              > Thread 3  [ x ]
    //      > Package 2:        [   ]
    //          > Process 4:    [   ]
    //              > Thread 5  [   ]
    //              > Thread 6  [   ]
    {
      Builder builder(0);

      builder.AddPackage(1);
      builder.AddProcess(0, 0);
      builder.AddThread(1);
      builder.AddThread(2);
      builder.AddThread(3);

      builder.AddPackage(2);
      builder.AddProcess(0, 4);
      builder.AddThread(5);
      builder.AddThread(6);

      serial_trace_packets[0] = builder.Build();
    }

    // Update our existing package with new processes, but no threads. Because
    // there are no new threads, the process tree should have no threads.
    //
    //  > Track Packet 1
    //      > Package 1:
    //          > Process 7:  [ x ]
    //      > Package 3:      [   ]
    //          > Process 8:  [   ]
    {
      Builder builder(1);

      builder.AddPackage(1);
      builder.AddProcess(0, 7);

      builder.AddPackage(3);
      builder.AddProcess(0, 8);

      serial_trace_packets[1] = builder.Build();
    }

    // Update our existing package with a new process, everything should be
    // discarded. Because everything will be discarded, we expect the process
    // tree to be discarded.
    //
    //  > Track Packet 2
    //      > Package 4:
    //          > Process 9:    [   ]
    //              > Thread 10 [   ]
    //              > Thread 11 [   ]
    {
      Builder builder(2);

      builder.AddPackage(4);
      builder.AddProcess(0, 9);
      builder.AddThread(10);
      builder.AddThread(11);

      serial_trace_packets[2] = builder.Build();
    }

    // This must be called after we have populated serial_trace_packets.
    context_.timeline = BuildTimeline();
  }

  int32_t ReadPid(protozero::ConstBytes bytes) const {
    protos::pbzero::ProcessTree::Process::Decoder decoder(bytes);

    return decoder.pid();
  }

  int32_t ReadTid(protozero::ConstBytes bytes) const {
    return protos::pbzero::ProcessTree::Thread::Decoder(bytes).tid();
  }

  // Returns all processes in this packet's process tree. This assumes there is
  // a process tree.
  std::vector<int32_t> CollectProcesses(const std::string& packet) const {
    protos::pbzero::TracePacket::Decoder packet_decoder(packet);
    protos::pbzero::ProcessTree::Decoder process_tree(
        packet_decoder.process_tree());

    std::vector<int32_t> list;

    for (auto it = process_tree.processes(); it; ++it) {
      list.push_back(ReadPid(it->as_bytes()));
    }

    std::sort(list.begin(), list.end());

    return list;
  }

  // Returns all threads in this packet's process tree. This assumes there is a
  // process tree.
  std::vector<int32_t> CollectThreads(const std::string& packet) const {
    protos::pbzero::TracePacket::Decoder packet_decoder(packet);
    protos::pbzero::ProcessTree::Decoder process_tree(
        packet_decoder.process_tree());

    std::vector<int32_t> list;

    for (auto it = process_tree.threads(); it; ++it) {
      list.push_back(ReadTid(it->as_bytes()));
    }

    std::sort(list.begin(), list.end());

    return list;
  }

  Context context_;

  std::array<std::string, 3> serial_trace_packets;

  const std::array<int32_t, 4> synth_threads = {1000, 1001, 1002, 1003};

 private:
  // Populate a new timeline using all serialized trace packets.
  std::unique_ptr<ProcessThreadTimeline> BuildTimeline() {
    auto timeline = std::make_unique<ProcessThreadTimeline>();

    for (auto& packet : serial_trace_packets) {
      protos::pbzero::TracePacket::Decoder trace_packet(packet);
      protos::pbzero::ProcessTree::Decoder process_tree(
          trace_packet.process_tree());
      AppendTimeline(trace_packet.timestamp(), process_tree, *timeline);
    }

    timeline->Sort();

    return timeline;
  }

  void AppendTimeline(uint64_t timestamp,
                      const protos::pbzero::ProcessTree::Decoder& packet,
                      ProcessThreadTimeline& timeline) const {
    for (auto it = packet.processes(); it; ++it) {
      protos::pbzero::ProcessTree::Process::Decoder process_decoder(*it);
      auto uid = static_cast<uint64_t>(process_decoder.uid());
      auto event = ProcessThreadTimeline::Event::Open(
          timestamp, process_decoder.pid(), process_decoder.ppid(), uid);
      timeline.Append(event);
    }

    for (auto it = packet.threads(); it; ++it) {
      protos::pbzero::ProcessTree::Thread::Decoder thread_decoder(*it);
      auto event = ProcessThreadTimeline::Event::Open(
          timestamp, thread_decoder.tid(), thread_decoder.tgid());
      timeline.Append(event);
    }
  }
};

TEST_F(ProcessTreeEndToEndTest, ProcessTreeShouldHaveNoProcesses) {
  // Collect all thread information from all packets, as if they were all in the
  // same trace.
  {
    ProcessTreeCollectThreads primitive;

    for (auto& packet : serial_trace_packets) {
      protos::pbzero::TracePacket::Decoder decoder(packet);
      ASSERT_OK(primitive.Collect(decoder, &context_));
    }
  }

  // Only copy processes/threads if they appear in the target package.
  {
    ProcessTreeCreateMask primitive;
    primitive.set_filter(std::make_unique<ConnectedToPackage>());
    ASSERT_OK(primitive.Build(&context_));
  }

  // Adding synth processes/threads must happen after ProcessTreeFilterThreads.
  // If it was called before, the synth threads would be dropped because they
  // are not connected to the target package.
  {
    ProcessTreeAddSynthThreads primitive;
    ASSERT_OK(primitive.Build(&context_));
  }

  ProcessTreeReplaceTrees replace_primitive;

  {
    auto packet = serial_trace_packets[0];
    ASSERT_OK(replace_primitive.Transform(context_, &packet));

    protos::pbzero::TracePacket::Decoder packet_decoder(packet);
    ASSERT_TRUE(packet_decoder.has_process_tree());

    protos::pbzero::ProcessTree::Decoder process_tree(
        packet_decoder.process_tree());

    ASSERT_TRUE(process_tree.has_processes());
    ASSERT_TRUE(process_tree.has_threads());

    auto processes = CollectProcesses(packet);
    auto threads = CollectThreads(packet);

    std::sort(processes.begin(), processes.end());
    std::sort(threads.begin(), threads.end());

    //  > Track Packet 0
    //      > Package 0:
    //          > Process 0:    [ x ]
    //              > Thread 1  [ x ]
    //              > Thread 2  [ x ]
    //              > Thread 3  [ x ]
    //      > Package 1:        [   ]
    //          > Process 4:    [   ]
    //              > Thread 5  [   ]
    //              > Thread 6  [   ]

    ASSERT_EQ(processes.size(), 2u);
    ASSERT_EQ(processes[0], 0);
    ASSERT_EQ(processes[1], synth_threads[0]);

    // kSynthThreads[1] does not appear here because it acts as the process id.
    // Synth threads will only appear in the first trace packet.
    ASSERT_EQ(threads.size(), 6u);
    ASSERT_EQ(threads[0], 1);
    ASSERT_EQ(threads[1], 2);
    ASSERT_EQ(threads[2], 3);
    ASSERT_EQ(threads[3], synth_threads[1]);
    ASSERT_EQ(threads[4], synth_threads[2]);
    ASSERT_EQ(threads[5], synth_threads[3]);
  }

  {
    auto packet = serial_trace_packets[0];
    replace_primitive.Transform(context_, &packet);

    protos::pbzero::TracePacket::Decoder packet_decoder(packet);
    ASSERT_TRUE(packet_decoder.has_process_tree());

    protos::pbzero::ProcessTree::Decoder process_tree(
        packet_decoder.process_tree());

    ASSERT_TRUE(process_tree.has_processes());
    ASSERT_TRUE(process_tree.has_threads());

    auto processes = CollectProcesses(packet);
    auto threads = CollectThreads(packet);

    std::sort(processes.begin(), processes.end());
    std::sort(threads.begin(), threads.end());

    // Update our existing package with new processes, but no threads. Because
    // there are no new threads, the process tree should have no threads.
    //
    //  > Track Packet 1
    //      > Package 0:
    //          > Process 7:  [ x ]
    //      > Package 2:      [   ]
    //          > Process 8:  [   ]

    ASSERT_EQ(processes.size(), 1u);
    ASSERT_EQ(processes[0], 7);

    ASSERT_EQ(threads.size(), 0u);
  }
}
}  // namespace perfetto::trace_redaction
