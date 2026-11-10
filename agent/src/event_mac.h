// Copyright 2022 The Chromium Authors.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_ANALYSIS_SRC_EVENT_MAC_H_
#define CONTENT_ANALYSIS_SRC_EVENT_MAC_H_

#include <memory>

#include "event_posix.h"

namespace content_analysis {
namespace sdk {

// ContentAnalysisEvent implementation for macOS; identical to POSIX.
class ContentAnalysisEventMac : public ContentAnalysisEventPosix {
 public:
  ContentAnalysisEventMac(const BrowserInfo& browser_info,
                          ContentAnalysisRequest request);
  ContentAnalysisEventMac(std::shared_ptr<ConnectionChannel> channel,
                          const BrowserInfo& browser_info,
                          ContentAnalysisRequest request);
};

}  // namespace sdk
}  // namespace content_analysis

#endif  // CONTENT_ANALYSIS_SRC_EVENT_MAC_H_
