// Copyright 2022 The Chromium Authors.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "event_mac.h"

namespace content_analysis {
namespace sdk {

ContentAnalysisEventMac::ContentAnalysisEventMac(
    const BrowserInfo& browser_info,
    ContentAnalysisRequest request)
    : ContentAnalysisEventPosix(browser_info, std::move(request)) {}

ContentAnalysisEventMac::ContentAnalysisEventMac(
    std::shared_ptr<ConnectionChannel> channel,
    const BrowserInfo& browser_info,
    ContentAnalysisRequest request)
    : ContentAnalysisEventPosix(std::move(channel), browser_info,
                                std::move(request)) {}

}  // namespace sdk
}  // namespace content_analysis
