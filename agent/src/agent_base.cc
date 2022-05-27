// Copyright 2022 The Chromium Authors.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "agent_base.h"

namespace content_analysis {
namespace sdk {

AgentBase::AgentBase(Config config, std::unique_ptr<AgentEventHandler> handler)
    : config_(std::move(config)), handler_(std::move(handler)) {}

const Agent::Config& AgentBase::GetConfig() const {
  return config_;
}

int AgentBase::Stop() {
  return 0;
}

}  // namespace sdk
}  // namespace content_analysis
