// Copyright 2022 The Chromium Authors.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_ANALYSIS_SRC_AGENT_POSIX_H_
#define CONTENT_ANALYSIS_SRC_AGENT_POSIX_H_

#include <atomic>
#include <memory>
#include <string>
#include <vector>

#include "agent_base.h"
#include "event_posix.h"

namespace content_analysis {
namespace sdk {

// Agent implementation for POSIX systems (Linux, and macOS through AgentMac).
// Listens on a Unix domain socket and serves any number of connected Google
// Chrome browser processes from one thread with poll().  See
// common/utils_posix.h for the channel layout and framing.
class AgentPosix : public AgentBase {
 public:
  AgentPosix(Config config,
             std::unique_ptr<AgentEventHandler> handler,
             ResultCode* rc);
  ~AgentPosix() override;

  // Agent:
  ResultCode HandleEvents() override;
  ResultCode Stop() override;
  std::string DebugString() const override;

 private:
  // One connected browser.  Incoming bytes are accumulated until a whole
  // length-prefixed message has arrived.
  struct Connection {
    std::shared_ptr<ConnectionChannel> channel;
    BrowserInfo browser_info;
    std::string buffer;    // Header, then header plus body.
    size_t expected = 0;   // Total bytes of the message being read, 0 while
                           // the header is incomplete.
  };

  ResultCode Listen();
  void AcceptConnection();
  // Returns false if the connection must be closed.
  bool ReadFromConnection(Connection& connection);
  ResultCode DispatchMessage(Connection& connection, const std::string& bytes);
  void CloseConnection(Connection& connection);
  void CloseAll();

  std::string socket_path_;
  int listen_fd_ = -1;
  int stop_pipe_[2] = {-1, -1};
  std::atomic<bool> stopped_{false};
  std::vector<Connection> connections_;
};

}  // namespace sdk
}  // namespace content_analysis

#endif  // CONTENT_ANALYSIS_SRC_AGENT_POSIX_H_
