// Copyright 2022 The Chromium Authors.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <fstream>
#include <iostream>
#include <string>

#include "content_analysis/sdk/analysis_agent.h"
#include "demo/handler.h"

int main(int argc, char* argv[]) {
  // Each agent uses a unique name to identify itself with Google Chrome.
  auto agent = content_analysis::sdk::Agent::Create({"content_analysis_sdk"},
      std::make_unique<Handler>());
  if (!agent) {
    std::cout << "[Demo] Error starting agent" << std::endl;
    return 1;
  };

  // Blocks, sending events to the handler until agent->Stop() is called.
  agent->HandleEvents();

  return 0;
};