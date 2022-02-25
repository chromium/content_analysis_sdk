// Copyright 2022 The Chromium Authors.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_ANALYSIS_AGENT_SRC_AGENT_BASE_H_
#define CONTENT_ANALYSIS_AGENT_SRC_AGENT_BASE_H_

#include "content_analysis/sdk/analysis_agent.h"

namespace content_analysis {
namespace sdk {

// Base Agent class with code common to all platforms.
class AgentBase : public Agent {
 public:
  // Agent:
  const Uri& GetUri() const override;
  int Stop() override;

 protected:
  AgentBase(const Uri& uri);

 private:
  Uri uri_;
};

}  // namespace sdk
}  // namespace content_analysis

#endif  // CONTENT_ANALYSIS_AGENT_SRC_AGENT_BASE_H_