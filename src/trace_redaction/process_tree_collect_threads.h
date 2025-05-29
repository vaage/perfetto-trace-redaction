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

#ifndef SRC_TRACE_REDACTION_PROCESS_TREE_COLLECT_THREADS_H_
#define SRC_TRACE_REDACTION_PROCESS_TREE_COLLECT_THREADS_H_

#include "perfetto/base/status.h"
#include "src/trace_redaction/trace_redaction_framework.h"

namespace perfetto::trace_redaction {

// Collects ProcessTrees::Process and ProcessTrees::Thread for each process and
// thread in a protos::pbzero::ProcessTree and writes them to the context.
class ProcessTreeCollectThreads : public CollectPrimitive {
 public:
  virtual base::Status Begin(Context*) const override;

  virtual base::Status Collect(const protos::pbzero::TracePacket::Decoder&,
                               Context*) const override;
};

}  // namespace perfetto::trace_redaction

#endif  // SRC_TRACE_REDACTION_PROCESS_TREE_COLLECT_THREADS_H_
