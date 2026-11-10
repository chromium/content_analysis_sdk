// Copyright 2026 The Chromium Authors.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_ANALYSIS_COMMON_UTILS_POSIX_H_
#define CONTENT_ANALYSIS_COMMON_UTILS_POSIX_H_

#include <sys/types.h>

#include <cstddef>
#include <cstdint>
#include <string>

// Helpers shared by the browser and agent sides of the Unix domain socket
// transport used on macOS and Linux.
//
// Channel layout.  Both sides derive the same socket path from the channel
// name in Agent::Config / Client::Config:
//
// - A system-wide agent (`user_specific == false`) listens at
//   `/var/run/<name>.sock`.  Only root can create files there, so an
//   unprivileged process cannot impersonate the agent; the socket itself is
//   made world-connectable so that browsers of every OS user can reach it.
// - A per-user agent (`user_specific == true`) listens in the calling user's
//   private runtime directory: `confstr(_CS_DARWIN_USER_TEMP_DIR)` on macOS
//   and `$XDG_RUNTIME_DIR` on Linux.  Both are mode 0700 and owned by the
//   user, so no other user can connect to or replace the socket.  If no such
//   directory exists the channel is invalid; there is no fallback to a
//   world-writable directory.
// - A name that starts with '/' is used verbatim as the socket path.  This is
//   for tests and controlled deployments; Chromium's provider table does not
//   use it.
//
// Framing.  Named pipes on Windows are message oriented, stream sockets are
// not, so every message is prefixed with its length as a 32-bit little-endian
// unsigned integer.  A message is one serialized `ChromeToAgent` (browser to
// agent) or `AgentToChrome` (agent to browser).  Zero-length messages and
// messages longer than `kMaxMessageSize` are protocol errors; the receiver
// closes the connection.

namespace content_analysis {
namespace sdk {
namespace internal {

// Largest message either side accepts.  Pasted text is sent inline, files
// are sent by path, so this comfortably exceeds the browser's limits.
constexpr size_t kMaxMessageSize = 64u * 1024u * 1024u;

// Size of the length prefix written before every message.
constexpr size_t kFrameHeaderSize = 4;

// Returns the socket path for `name`, or an empty string if the name is not
// a valid channel name or no suitable directory exists for the user.
std::string GetSocketPath(const std::string& name, bool user_specific);

// Returns true if `name` is acceptable as a channel name: either an absolute
// path that fits in sockaddr_un, or a short token of [A-Za-z0-9._-] that does
// not start with '.'.
bool IsValidChannelName(const std::string& name);

// Creates a connected or listening AF_UNIX stream socket with close-on-exec
// set and SIGPIPE suppressed where the platform supports it.  Returns -1 and
// sets errno on failure.
int CreateUnixSocket();

// Writes `message` with its length prefix.  Blocks until everything is
// written.  Returns 0 on success or the errno of the failing call.  Returns
// EMSGSIZE for an empty or oversized message.
int WriteFramedMessage(int fd, const std::string& message);

// Reads one length-prefixed message into `message`.  Blocks until the whole
// message has arrived.  Returns 0 on success, ECONNRESET if the peer closed
// the connection, EMSGSIZE for an invalid length, or the errno of the failing
// read.
int ReadFramedMessage(int fd, std::string* message);

// Reads or writes exactly `size` bytes, retrying on EINTR and short
// transfers.  Return 0 on success, ECONNRESET on end of stream, else errno.
int ReadAll(int fd, char* data, size_t size);
int WriteAll(int fd, const char* data, size_t size);

// Retrieves the process id of the peer connected to `fd`.
bool GetPeerPid(int fd, pid_t* pid);

// Retrieves the full path of the main binary of process `pid`.
bool GetProcessPath(pid_t pid, std::string* binary_path);

// Encodes and decodes the length prefix.
void EncodeFrameHeader(uint32_t size, char* out);
uint32_t DecodeFrameHeader(const char* in);

}  // namespace internal
}  // namespace sdk
}  // namespace content_analysis

#endif  // CONTENT_ANALYSIS_COMMON_UTILS_POSIX_H_
