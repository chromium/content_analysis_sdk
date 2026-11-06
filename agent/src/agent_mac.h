// Copyright 2022 The Chromium Authors.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_ANALYSIS_SRC_AGENT_MAC_H_
#define CONTENT_ANALYSIS_SRC_AGENT_MAC_H_

#include "agent_posix.h"

namespace content_analysis {
namespace sdk {

// Agent implementation for macOS: the POSIX Unix domain socket agent with
// the macOS per-user socket directory (see common/utils_posix.cc).
class AgentMac : public AgentPosix {
 public:
  AgentMac(Config config,
           std::unique_ptr<AgentEventHandler> handler,
           ResultCode* rc);
};

}  // namespace sdk
}  // namespace content_analysis

#endif  // CONTENT_ANALYSIS_SRC_AGENT_MAC_H_
