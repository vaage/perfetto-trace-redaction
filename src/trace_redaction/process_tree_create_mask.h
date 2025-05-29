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

#ifndef SRC_TRACE_REDACTION_PROCESS_TREE_CREATE_MASK_H_
#define SRC_TRACE_REDACTION_PROCESS_TREE_CREATE_MASK_H_

#include "perfetto/base/status.h"
#include "src/trace_redaction/redact_sched_events.h"
#include "src/trace_redaction/trace_redaction_framework.h"

namespace perfetto::trace_redaction {

class ProcessTreeCreateMask : public BuildPrimitive {
 public:
  virtual base::Status Build(Context* context) const override;

  // Sets the filter that controls which processes/threads will be included
  // after Build() is called. In production, this will use "connected to target
  // package", but in tests it may be something simpler.
  template <class Filter>
  void set_filter(std::unique_ptr<Filter> filter) {
    filter_ = std::move(filter);
  }

 private:
  std::unique_ptr<PidFilter> filter_;
};

}  // namespace perfetto::trace_redaction

#endif  // SRC_TRACE_REDACTION_PROCESS_TREE_CREATE_MASK_H_
