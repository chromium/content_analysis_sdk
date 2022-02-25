// Copyright 2022 The Chromium Authors.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "agent_posix.h"
#include "session_posix.h"

namespace content_analysis {
namespace sdk {

// static
std::unique_ptr<Agent> Agent::Create(const Uri& uri) {
  return std::make_unique<AgentPosix>(uri);
}

AgentPosix::AgentPosix(const Uri& uri) : AgentBase(uri) {}

std::unique_ptr<Session> AgentPosix::GetNextSession() {
  return std::make_unique<SessionPosix>();
}

}  // namespace sdk
}  // namespace content_analysis
