// Copyright 2022 The Chromium Authors.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "agent_mac.h"
#include "session_mac.h"

namespace content_analysis {
namespace sdk {

// static
std::unique_ptr<Agent> Agent::Create(const Uri& uri) {
  return std::make_unique<AgentMac>(uri);
}

AgentMac::AgentMac(const Uri& uri) : AgentBase(uri) {}

std::unique_ptr<Session> AgentMac::GetNextSession() {
  return std::make_unique<SessionMac>();
}

}  // namespace sdk
}  // namespace content_analysis
