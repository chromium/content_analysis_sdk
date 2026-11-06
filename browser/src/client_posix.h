// Copyright 2022 The Chromium Authors.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_ANALYSIS_BROWSER_SRC_CLIENT_POSIX_H_
#define CONTENT_ANALYSIS_BROWSER_SRC_CLIENT_POSIX_H_

#include <mutex>
#include <string>

#include "client_base.h"

namespace content_analysis {
namespace sdk {

// Client implementation for POSIX systems (Linux, and macOS through
// ClientMac).  One instance owns one connected Unix domain socket to the
// agent.  See common/utils_posix.h for the channel layout and framing.
class ClientPosix : public ClientBase {
 public:
  explicit ClientPosix(Config config);
  ~ClientPosix() override;

  // Returns true if the socket connected and the agent could be identified.
  bool IsConnected() const { return fd_ >= 0; }

  // Client:
  int Send(ContentAnalysisRequest request,
           ContentAnalysisResponse* response) override;
  int Acknowledge(const ContentAnalysisAcknowledgement& ack) override;
  int CancelRequests(const ContentAnalysisCancelRequests& cancel) override;

 private:
  // Connects to the agent and fills in the agent info.  On failure the
  // socket is closed and `fd_` is -1.
  void Connect();
  void Shutdown();

  // Writes one ChromeToAgent message.  Returns 0 on success, -1 on failure.
  int WriteMessage(const ChromeToAgent& message);

  // Serializes Send(), Acknowledge() and CancelRequests() on the socket so
  // that a request and its response are never interleaved with another
  // message.  Google Chrome may call these from more than one sequence.
  std::mutex io_mutex_;
  int fd_ = -1;
};

}  // namespace sdk
}  // namespace content_analysis

#endif  // CONTENT_ANALYSIS_BROWSER_SRC_CLIENT_POSIX_H_
