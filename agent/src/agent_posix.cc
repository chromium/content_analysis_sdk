// Copyright 2022 The Chromium Authors.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "agent_posix.h"

#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <unistd.h>

#include <cstring>
#include <sstream>
#include <utility>

#include "common/utils_posix.h"

namespace content_analysis {
namespace sdk {

namespace {

// Pending connections queued by the kernel before accept().  Browsers
// connect once per process, so this only matters at login storms.
constexpr int kListenBacklog = 16;

// Largest read per poll() wake-up.
constexpr size_t kReadChunk = 64 * 1024;

bool SetNonBlocking(int fd) {
  int flags = fcntl(fd, F_GETFL);
  return flags >= 0 && fcntl(fd, F_SETFL, flags | O_NONBLOCK) >= 0;
}

bool SetCloseOnExec(int fd) {
  int flags = fcntl(fd, F_GETFD);
  return flags >= 0 && fcntl(fd, F_SETFD, flags | FD_CLOEXEC) >= 0;
}

// Returns true if a live agent already answers at `address`.
bool AgentIsListening(const sockaddr_un& address) {
  int fd = internal::CreateUnixSocket();
  if (fd < 0) {
    return false;
  }
  int rc = connect(fd, reinterpret_cast<const sockaddr*>(&address),
                   sizeof(address));
  close(fd);
  return rc == 0;
}

}  // namespace

#if !defined(__APPLE__)
// static
std::unique_ptr<Agent> Agent::Create(
    Config config,
    std::unique_ptr<AgentEventHandler> handler,
    ResultCode* rc) {
  auto agent = std::make_unique<AgentPosix>(std::move(config),
                                            std::move(handler), rc);
  return *rc == ResultCode::OK ? std::move(agent) : nullptr;
}
#endif

AgentPosix::AgentPosix(Config config,
                       std::unique_ptr<AgentEventHandler> event_handler,
                       ResultCode* rc)
    : AgentBase(std::move(config), std::move(event_handler)) {
  *rc = ResultCode::OK;
  if (handler() == nullptr) {
    *rc = ResultCode::ERR_AGENT_EVENT_HANDLER_NOT_SPECIFIED;
    return;
  }

  if (pipe(stop_pipe_) != 0 || !SetCloseOnExec(stop_pipe_[0]) ||
      !SetCloseOnExec(stop_pipe_[1]) || !SetNonBlocking(stop_pipe_[0]) ||
      !SetNonBlocking(stop_pipe_[1])) {
    *rc = ResultCode::ERR_CANNOT_CREATE_AGENT_STOP_EVENT;
    return;
  }

  socket_path_ = internal::GetSocketPath(configuration().name,
                                         configuration().user_specific);
  if (socket_path_.empty()) {
    *rc = ResultCode::ERR_INVALID_CHANNEL_NAME;
    return;
  }

  *rc = Listen();
}

AgentPosix::~AgentPosix() {
  CloseAll();
  if (listen_fd_ >= 0) {
    close(listen_fd_);
    unlink(socket_path_.c_str());
  }
  for (int fd : stop_pipe_) {
    if (fd >= 0) {
      close(fd);
    }
  }
}

ResultCode AgentPosix::Listen() {
  sockaddr_un address;
  memset(&address, 0, sizeof(address));
  address.sun_family = AF_UNIX;
  strncpy(address.sun_path, socket_path_.c_str(), sizeof(address.sun_path) - 1);

  // A socket file left behind by a crashed agent must be replaced, but a
  // running agent must not be displaced.  This mirrors
  // FILE_FLAG_FIRST_PIPE_INSTANCE on Windows.
  struct stat info;
  if (lstat(socket_path_.c_str(), &info) == 0) {
    if (!S_ISSOCK(info.st_mode)) {
      return ResultCode::ERR_INVALID_CHANNEL_NAME;
    }
    if (AgentIsListening(address)) {
      return ResultCode::ERR_AGENT_ALREADY_EXISTS;
    }
    unlink(socket_path_.c_str());
  }

  listen_fd_ = internal::CreateUnixSocket();
  if (listen_fd_ < 0) {
    return ResultCode::ERR_UNEXPECTED;
  }

  // Create the socket closed to everyone, then open it exactly as far as the
  // deployment needs: every local user for a system agent, only the owner
  // for a per-user agent.  The parent directory already limits who may
  // create the file (see common/utils_posix.h).
  mode_t previous = umask(0077);
  int rc = bind(listen_fd_, reinterpret_cast<const sockaddr*>(&address),
                sizeof(address));
  umask(previous);
  if (rc != 0) {
    int err = errno;
    close(listen_fd_);
    listen_fd_ = -1;
    return err == EADDRINUSE ? ResultCode::ERR_AGENT_ALREADY_EXISTS
                             : ResultCode::ERR_INVALID_CHANNEL_NAME;
  }

  mode_t mode = configuration().user_specific ? 0600 : 0666;
  if (chmod(socket_path_.c_str(), mode) != 0 || !SetNonBlocking(listen_fd_) ||
      listen(listen_fd_, kListenBacklog) != 0) {
    close(listen_fd_);
    listen_fd_ = -1;
    unlink(socket_path_.c_str());
    return ResultCode::ERR_UNEXPECTED;
  }

  return ResultCode::OK;
}

ResultCode AgentPosix::HandleEvents() {
  if (listen_fd_ < 0) {
    return ResultCode::ERR_AGENT_NOT_INITIALIZED;
  }

  std::vector<pollfd> fds;
  while (!stopped_.load()) {
    fds.clear();
    fds.push_back({stop_pipe_[0], POLLIN, 0});
    fds.push_back({listen_fd_, POLLIN, 0});
    for (const Connection& connection : connections_) {
      fds.push_back({connection.channel->fd, POLLIN, 0});
    }

    int ready = poll(fds.data(), fds.size(), -1);
    if (ready < 0) {
      if (errno == EINTR) {
        continue;
      }
      return NotifyError("poll", ResultCode::ERR_UNEXPECTED);
    }

    if (fds[0].revents != 0) {
      break;
    }

    // Walk connections before accepting so indexes stay aligned with `fds`.
    // Closed connections are compacted afterwards.
    for (size_t i = 0; i < connections_.size(); ++i) {
      short revents = fds[i + 2].revents;
      if (revents == 0) {
        continue;
      }
      if ((revents & POLLIN) && !ReadFromConnection(connections_[i])) {
        CloseConnection(connections_[i]);
      } else if (revents & (POLLHUP | POLLERR | POLLNVAL)) {
        CloseConnection(connections_[i]);
      }
    }
    for (size_t i = 0; i < connections_.size();) {
      if (connections_[i].channel->fd < 0) {
        connections_.erase(connections_.begin() + i);
      } else {
        ++i;
      }
    }

    if (fds[1].revents & POLLIN) {
      AcceptConnection();
    }
  }

  CloseAll();
  return ResultCode::OK;
}

ResultCode AgentPosix::Stop() {
  stopped_.store(true);
  if (stop_pipe_[1] >= 0) {
    char byte = 0;
    ssize_t written;
    do {
      written = write(stop_pipe_[1], &byte, 1);
    } while (written < 0 && errno == EINTR);
  }
  return ResultCode::OK;
}

void AgentPosix::AcceptConnection() {
  int fd;
  do {
    fd = accept(listen_fd_, nullptr, nullptr);
  } while (fd < 0 && errno == EINTR);
  if (fd < 0) {
    if (errno != EAGAIN && errno != EWOULDBLOCK) {
      NotifyError("accept", ResultCode::ERR_UNEXPECTED);
    }
    return;
  }
  if (!SetCloseOnExec(fd) || !SetNonBlocking(fd)) {
    close(fd);
    NotifyError("accept", ResultCode::ERR_UNEXPECTED);
    return;
  }
#if defined(SO_NOSIGPIPE)
  int one = 1;
  setsockopt(fd, SOL_SOCKET, SO_NOSIGPIPE, &one, sizeof(one));
#endif

  Connection connection;
  pid_t pid = 0;
  if (!internal::GetPeerPid(fd, &pid)) {
    close(fd);
    NotifyError("BuildBrowserInfo", ResultCode::ERR_CANNOT_GET_BROWSER_PID);
    return;
  }
  connection.browser_info.pid = static_cast<unsigned long>(pid);
  if (!internal::GetProcessPath(pid, &connection.browser_info.binary_path)) {
    close(fd);
    NotifyError("BuildBrowserInfo",
                ResultCode::ERR_CANNOT_GET_BROWSER_BINARY_PATH);
    return;
  }

  connection.channel = std::make_shared<ConnectionChannel>();
  connection.channel->fd = fd;
  connection.buffer.reserve(internal::kFrameHeaderSize);
  connections_.push_back(std::move(connection));
  handler()->OnBrowserConnected(connections_.back().browser_info);
}

bool AgentPosix::ReadFromConnection(Connection& connection) {
  char chunk[kReadChunk];
  ssize_t count;
  do {
    count = read(connection.channel->fd, chunk, sizeof(chunk));
  } while (count < 0 && errno == EINTR);
  if (count == 0) {
    return false;  // Browser closed the connection.
  }
  if (count < 0) {
    return errno == EAGAIN || errno == EWOULDBLOCK;
  }

  connection.buffer.append(chunk, static_cast<size_t>(count));

  // Several messages may arrive in one read; dispatch each complete one.
  while (true) {
    if (connection.expected == 0) {
      if (connection.buffer.size() < internal::kFrameHeaderSize) {
        return true;
      }
      uint32_t size = internal::DecodeFrameHeader(connection.buffer.data());
      if (size == 0 || size > internal::kMaxMessageSize) {
        NotifyError("FrameHeader", ResultCode::ERR_INVALID_REQUEST_FROM_BROWSER);
        return false;
      }
      connection.expected = internal::kFrameHeaderSize + size;
    }
    if (connection.buffer.size() < connection.expected) {
      return true;
    }

    std::string bytes = connection.buffer.substr(
        internal::kFrameHeaderSize,
        connection.expected - internal::kFrameHeaderSize);
    connection.buffer.erase(0, connection.expected);
    connection.expected = 0;

    if (DispatchMessage(connection, bytes) ==
        ResultCode::ERR_INVALID_REQUEST_FROM_BROWSER) {
      return false;
    }
  }
}

ResultCode AgentPosix::DispatchMessage(Connection& connection,
                                       const std::string& bytes) {
  ChromeToAgent message;
  if (!message.ParseFromString(bytes)) {
    return NotifyError("ParseChromeToAgent",
                       ResultCode::ERR_INVALID_REQUEST_FROM_BROWSER);
  }

  if (message.has_request()) {
    auto event = std::make_unique<ContentAnalysisEventPosix>(
        connection.channel, connection.browser_info,
        std::move(*message.mutable_request()));
    ResultCode rc = event->Init();
    if (rc != ResultCode::OK) {
      return NotifyError("RequestValidation", rc);
    }
    handler()->OnAnalysisRequested(std::move(event));
    return ResultCode::OK;
  }
  if (message.has_ack()) {
    handler()->OnResponseAcknowledged(message.ack());
    return ResultCode::OK;
  }
  if (message.has_cancel()) {
    handler()->OnCancelRequests(message.cancel());
    return ResultCode::OK;
  }
  return NotifyError("NoRequestOrAck",
                     ResultCode::ERR_INVALID_REQUEST_FROM_BROWSER);
}

void AgentPosix::CloseConnection(Connection& connection) {
  int fd;
  {
    std::lock_guard<std::mutex> lock(connection.channel->write_mutex);
    fd = connection.channel->fd;
    connection.channel->fd = -1;
  }
  if (fd >= 0) {
    close(fd);
    handler()->OnBrowserDisconnected(connection.browser_info);
  }
}

void AgentPosix::CloseAll() {
  for (Connection& connection : connections_) {
    CloseConnection(connection);
  }
  connections_.clear();
}

std::string AgentPosix::DebugString() const {
  std::stringstream state;
  state << "AgentPosix{path=" << socket_path_;
  state << " listen_fd=" << listen_fd_;
  state << " connections=" << connections_.size();
  state << " stopped=" << stopped_.load();
  state << "}";
  return state.str();
}

}  // namespace sdk
}  // namespace content_analysis
