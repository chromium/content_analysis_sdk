// Copyright 2022 The Chromium Authors.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "agent_mac.h"

namespace content_analysis {
namespace sdk {

// static
std::unique_ptr<Agent> Agent::Create(
    Config config,
    std::unique_ptr<AgentEventHandler> handler,
    ResultCode* rc) {
  auto agent = std::make_unique<AgentMac>(std::move(config),
                                          std::move(handler), rc);
  return *rc == ResultCode::OK ? std::move(agent) : nullptr;
}

AgentMac::AgentMac(Config config,
                   std::unique_ptr<AgentEventHandler> handler,
                   ResultCode* rc)
    : AgentPosix(std::move(config), std::move(handler), rc) {}

}  // namespace sdk
}  // namespace content_analysis
