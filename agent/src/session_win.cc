// Copyright 2022 The Chromium Authors.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "session_win.h"

namespace content_analysis {
namespace sdk {

const DWORD kBufferSize = 4096;

SessionWin::SessionWin(HANDLE handle) : hPipe_(handle) {}

SessionWin::~SessionWin() {
  Shutdown();
}

DWORD SessionWin::Init() {
  std::vector<char> buffer = ReadNextMessageFromPipe(hPipe_);
  if (!request()->ParseFromArray(buffer.data(), buffer.size())) {
    return -1;
  }
  // TODO(rogerta): do some basic validation of the request.

  // Prepare the response so that ALLOW verdicts are the default().
  return UpdateResponse(*response(),
      request()->tags_size() > 0 ? request()->tags(0) : std::string(),
      ContentAnalysisResponse::Result::SUCCESS);
}

int SessionWin::Close() {
  Shutdown();
  return SessionBase::Close();
}

int SessionWin::Send() {
  std::string response_str;
  if (!response()->SerializeToString(&response_str))
    return -1;

  DWORD written;
  if (!WriteFile(hPipe_, response_str.data(), response_str.size(), &written,
                nullptr)) {
    return -1;
  }

  return 0;
}

void SessionWin::Shutdown() {
  if (hPipe_ != INVALID_HANDLE_VALUE) {
    FlushFileBuffers(hPipe_);
    CloseHandle(hPipe_);
    hPipe_ = INVALID_HANDLE_VALUE;
  }
}

// Reads the next message from the pipe and returns a buffer of chars.
// Can read any length of message.
std::vector<char> ReadNextMessageFromPipe(HANDLE pipe) {
  DWORD err = ERROR_SUCCESS;
  std::vector<char> buffer(kBufferSize);
  char* p = buffer.data();
  int final_size = 0;
  while (true) {
    DWORD read;
    if (ReadFile(pipe, p, kBufferSize, &read, nullptr)) {
      final_size += read;
      break;
    } else {
      err = GetLastError();
      if (err != ERROR_MORE_DATA)
        break;

      buffer.resize(buffer.size() + kBufferSize);
      p = buffer.data() + buffer.size() - kBufferSize;
    }
  }
  buffer.resize(final_size);
  return buffer;
}

}  // namespace sdk
}  // namespace content_analysis
