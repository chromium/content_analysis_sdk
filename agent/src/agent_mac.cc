// Copyright 2022 The Chromium Authors.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "agent_mac.h"
#include "event_mac.h"

namespace content_analysis {
namespace sdk {

// static
std::unique_ptr<Agent> Agent::Create(
    Config config,
    std::unique_ptr<AgentEventHandler> handler) {
  return std::make_unique<AgentMac>(std::move(config), std::move(handler));
}

AgentMac::AgentMac(
    Config config,
    std::unique_ptr<AgentEventHandler> handler)
  : AgentBase(std::move(config), std::move(handler)) {}

void AgentMac::HandleEvents() {}

std::string AgentMac::DebugString() const {
  return std::string();
}

}  // namespace sdk
}  // namespace content_analysis
