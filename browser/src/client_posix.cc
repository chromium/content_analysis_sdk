// Copyright 2022 The Chromium Authors.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "client_posix.h"

#include <errno.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include <cstring>
#include <utility>

#include "common/utils_posix.h"

namespace content_analysis {
namespace sdk {

#if !defined(__APPLE__)
// static
std::unique_ptr<Client> Client::Create(Config config) {
  auto client = std::make_unique<ClientPosix>(std::move(config));
  return client->IsConnected() ? std::move(client) : nullptr;
}
#endif

ClientPosix::ClientPosix(Config config) : ClientBase(std::move(config)) {
  Connect();
}

ClientPosix::~ClientPosix() {
  Shutdown();
}

void ClientPosix::Connect() {
  std::string path = internal::GetSocketPath(configuration().name,
                                             configuration().user_specific);
  if (path.empty()) {
    return;
  }

  sockaddr_un address;
  memset(&address, 0, sizeof(address));
  address.sun_family = AF_UNIX;
  strncpy(address.sun_path, path.c_str(), sizeof(address.sun_path) - 1);

  fd_ = internal::CreateUnixSocket();
  if (fd_ < 0) {
    return;
  }

  int rc;
  do {
    rc = connect(fd_, reinterpret_cast<const sockaddr*>(&address),
                 sizeof(address));
  } while (rc != 0 && errno == EINTR);
  if (rc != 0) {
    Shutdown();
    return;
  }

  // The browser verifies the agent's binary, so the connection is only
  // usable if the peer can be identified.
  pid_t pid = 0;
  if (!internal::GetPeerPid(fd_, &pid) ||
      !internal::GetProcessPath(pid, &agent_info().binary_path)) {
    Shutdown();
    return;
  }
  agent_info().pid = static_cast<unsigned long>(pid);
}

void ClientPosix::Shutdown() {
  if (fd_ >= 0) {
    close(fd_);
    fd_ = -1;
  }
}

int ClientPosix::WriteMessage(const ChromeToAgent& message) {
  if (fd_ < 0) {
    return -1;
  }
  std::string bytes;
  if (!message.SerializeToString(&bytes)) {
    return -1;
  }
  return internal::WriteFramedMessage(fd_, bytes) == 0 ? 0 : -1;
}

int ClientPosix::Send(ContentAnalysisRequest request,
                      ContentAnalysisResponse* response) {
  std::lock_guard<std::mutex> lock(io_mutex_);

  ChromeToAgent chrome_to_agent;
  *chrome_to_agent.mutable_request() = std::move(request);
  if (WriteMessage(chrome_to_agent) != 0) {
    return -1;
  }

  std::string bytes;
  if (internal::ReadFramedMessage(fd_, &bytes) != 0) {
    return -1;
  }

  AgentToChrome agent_to_chrome;
  if (!agent_to_chrome.ParseFromString(bytes) ||
      !agent_to_chrome.has_response()) {
    return -1;
  }

  *response = std::move(*agent_to_chrome.mutable_response());
  return 0;
}

int ClientPosix::Acknowledge(const ContentAnalysisAcknowledgement& ack) {
  std::lock_guard<std::mutex> lock(io_mutex_);
  ChromeToAgent chrome_to_agent;
  *chrome_to_agent.mutable_ack() = ack;
  return WriteMessage(chrome_to_agent);
}

int ClientPosix::CancelRequests(const ContentAnalysisCancelRequests& cancel) {
  std::lock_guard<std::mutex> lock(io_mutex_);
  ChromeToAgent chrome_to_agent;
  *chrome_to_agent.mutable_cancel() = cancel;
  return WriteMessage(chrome_to_agent);
}

}  // namespace sdk
}  // namespace content_analysis
