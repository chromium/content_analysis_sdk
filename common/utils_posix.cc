// Copyright 2026 The Chromium Authors.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "common/utils_posix.h"

#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include <cstring>

#if defined(__APPLE__)
#include <libproc.h>
#else
#include <stdlib.h>
#include <limits.h>
#endif

namespace content_analysis {
namespace sdk {
namespace internal {

namespace {

constexpr size_t kMaxSunPath = sizeof(sockaddr_un().sun_path) - 1;
constexpr size_t kMaxNameLength = 64;

std::string UserRuntimeDirectory() {
#if defined(__APPLE__)
  char buffer[PATH_MAX];
  size_t length = confstr(_CS_DARWIN_USER_TEMP_DIR, buffer, sizeof(buffer));
  if (length == 0 || length > sizeof(buffer)) {
    return std::string();
  }
  std::string directory(buffer);
#else
  const char* runtime = getenv("XDG_RUNTIME_DIR");
  if (runtime == nullptr || runtime[0] != '/') {
    return std::string();
  }
  std::string directory(runtime);
#endif
  if (directory.empty() || directory.back() != '/') {
    directory += '/';
  }
  return directory;
}

}  // namespace

bool IsValidChannelName(const std::string& name) {
  if (name.empty()) {
    return false;
  }
  if (name[0] == '/') {
    return name.size() <= kMaxSunPath &&
           name.find('\0') == std::string::npos;
  }
  if (name.size() > kMaxNameLength || name[0] == '.') {
    return false;
  }
  for (char c : name) {
    bool ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
              (c >= '0' && c <= '9') || c == '_' || c == '-' || c == '.';
    if (!ok) {
      return false;
    }
  }
  return true;
}

std::string GetSocketPath(const std::string& name, bool user_specific) {
  if (!IsValidChannelName(name)) {
    return std::string();
  }
  if (name[0] == '/') {
    return name;
  }

  std::string path;
  if (user_specific) {
    path = UserRuntimeDirectory();
    if (path.empty()) {
      return std::string();
    }
  } else {
    path = "/var/run/";
  }
  path += name;
  path += ".sock";
  return path.size() <= kMaxSunPath ? path : std::string();
}

int CreateUnixSocket() {
  int fd = socket(AF_UNIX, SOCK_STREAM, 0);
  if (fd < 0) {
    return -1;
  }
  int flags = fcntl(fd, F_GETFD);
  if (flags < 0 || fcntl(fd, F_SETFD, flags | FD_CLOEXEC) < 0) {
    int saved = errno;
    close(fd);
    errno = saved;
    return -1;
  }
#if defined(SO_NOSIGPIPE)
  int one = 1;
  setsockopt(fd, SOL_SOCKET, SO_NOSIGPIPE, &one, sizeof(one));
#endif
  return fd;
}

void EncodeFrameHeader(uint32_t size, char* out) {
  out[0] = static_cast<char>(size & 0xff);
  out[1] = static_cast<char>((size >> 8) & 0xff);
  out[2] = static_cast<char>((size >> 16) & 0xff);
  out[3] = static_cast<char>((size >> 24) & 0xff);
}

uint32_t DecodeFrameHeader(const char* in) {
  const unsigned char* bytes = reinterpret_cast<const unsigned char*>(in);
  return static_cast<uint32_t>(bytes[0]) |
         (static_cast<uint32_t>(bytes[1]) << 8) |
         (static_cast<uint32_t>(bytes[2]) << 16) |
         (static_cast<uint32_t>(bytes[3]) << 24);
}

int ReadAll(int fd, char* data, size_t size) {
  while (size > 0) {
    ssize_t count = read(fd, data, size);
    if (count == 0) {
      return ECONNRESET;
    }
    if (count < 0) {
      if (errno == EINTR) {
        continue;
      }
      return errno;
    }
    data += count;
    size -= static_cast<size_t>(count);
  }
  return 0;
}

int WriteAll(int fd, const char* data, size_t size) {
  while (size > 0) {
#if defined(MSG_NOSIGNAL)
    ssize_t count = send(fd, data, size, MSG_NOSIGNAL);
#else
    ssize_t count = write(fd, data, size);
#endif
    if (count < 0) {
      if (errno == EINTR) {
        continue;
      }
      if (errno == EAGAIN || errno == EWOULDBLOCK) {
        // The agent keeps its connection sockets non-blocking for its poll
        // loop; wait until the kernel buffer drains rather than fail.
        pollfd waiter = {fd, POLLOUT, 0};
        if (poll(&waiter, 1, -1) < 0 && errno != EINTR) {
          return errno;
        }
        continue;
      }
      return errno == EPIPE ? ECONNRESET : errno;
    }
    data += count;
    size -= static_cast<size_t>(count);
  }
  return 0;
}

int WriteFramedMessage(int fd, const std::string& message) {
  if (message.empty() || message.size() > kMaxMessageSize) {
    return EMSGSIZE;
  }
  char header[kFrameHeaderSize];
  EncodeFrameHeader(static_cast<uint32_t>(message.size()), header);
  int rc = WriteAll(fd, header, sizeof(header));
  if (rc != 0) {
    return rc;
  }
  return WriteAll(fd, message.data(), message.size());
}

int ReadFramedMessage(int fd, std::string* message) {
  char header[kFrameHeaderSize];
  int rc = ReadAll(fd, header, sizeof(header));
  if (rc != 0) {
    return rc;
  }
  uint32_t size = DecodeFrameHeader(header);
  if (size == 0 || size > kMaxMessageSize) {
    return EMSGSIZE;
  }
  message->resize(size);
  return ReadAll(fd, &(*message)[0], size);
}

bool GetPeerPid(int fd, pid_t* pid) {
#if defined(__APPLE__)
  pid_t value = 0;
  socklen_t length = sizeof(value);
  if (getsockopt(fd, SOL_LOCAL, LOCAL_PEERPID, &value, &length) != 0) {
    return false;
  }
  *pid = value;
  return true;
#else
  struct ucred credentials;
  socklen_t length = sizeof(credentials);
  if (getsockopt(fd, SOL_SOCKET, SO_PEERCRED, &credentials, &length) != 0) {
    return false;
  }
  *pid = credentials.pid;
  return true;
#endif
}

bool GetProcessPath(pid_t pid, std::string* binary_path) {
#if defined(__APPLE__)
  char buffer[PROC_PIDPATHINFO_MAXSIZE];
  int length = proc_pidpath(pid, buffer, sizeof(buffer));
  if (length <= 0) {
    return false;
  }
  binary_path->assign(buffer, static_cast<size_t>(length));
  return true;
#else
  std::string link = "/proc/" + std::to_string(pid) + "/exe";
  char buffer[PATH_MAX];
  ssize_t length = readlink(link.c_str(), buffer, sizeof(buffer) - 1);
  if (length <= 0) {
    return false;
  }
  binary_path->assign(buffer, static_cast<size_t>(length));
  return true;
#endif
}

}  // namespace internal
}  // namespace sdk
}  // namespace content_analysis
