// Copyright 2022 The Chromium Authors.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_ANALYSIS_SRC_EVENT_POSIX_H_
#define CONTENT_ANALYSIS_SRC_EVENT_POSIX_H_

#include <memory>
#include <mutex>

#include "event_base.h"

namespace content_analysis {
namespace sdk {

// The write side of one browser connection, shared between the agent's
// connection bookkeeping and the events created for that connection.  Events
// may be completed on any thread after the browser disconnected, so the
// descriptor is guarded by a mutex and invalidated by the agent on close.
struct ConnectionChannel {
  std::mutex write_mutex;
  int fd = -1;  // -1 once the browser disconnected.
};

// ContentAnalysisEvent implementation for POSIX systems (Linux, and macOS
// through ContentAnalysisEventMac).
class ContentAnalysisEventPosix : public ContentAnalysisEventBase {
 public:
  // Creates an event that cannot send a response.  Used by tests.
  ContentAnalysisEventPosix(const BrowserInfo& browser_info,
                            ContentAnalysisRequest request);
  ContentAnalysisEventPosix(std::shared_ptr<ConnectionChannel> channel,
                            const BrowserInfo& browser_info,
                            ContentAnalysisRequest request);
  ~ContentAnalysisEventPosix() override;

  // Validates the request and prepares the default allow response.
  ResultCode Init();

  // ContentAnalysisEvent:
  ResultCode Close() override;
  ResultCode Send() override;
  std::string DebugString() const override;

 private:
  void Shutdown();

  std::shared_ptr<ConnectionChannel> channel_;
  bool response_sent_ = false;
};

}  // namespace sdk
}  // namespace content_analysis

#endif  // CONTENT_ANALYSIS_SRC_EVENT_POSIX_H_
