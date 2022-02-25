// Copyright 2022 The Chromium Authors.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "agent_base.h"

namespace content_analysis {
namespace sdk {

AgentBase::AgentBase(const Uri& uri) : uri_(uri) {}

const Agent::Uri& AgentBase::GetUri() const {
  return uri_;
}

int AgentBase::Stop() {
  return 0;
}

}  // namespace sdk
}  // namespace content_analysis
