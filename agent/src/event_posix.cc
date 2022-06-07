// Copyright 2022 The Chromium Authors.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "event_posix.h"

namespace content_analysis {
namespace sdk {

  ContentAnalysisEventPosix::ContentAnalysisEventPosix(
    const BrowserInfo& browser_info,
    ContentAnalysisRequest req)
    : ContentAnalysisEventBase(browser_info) {
  *request() = std::move(req);
}

int ContentAnalysisEventPosix::Send() {
  return -1;
}

}  // namespace sdk
}  // namespace content_analysis
