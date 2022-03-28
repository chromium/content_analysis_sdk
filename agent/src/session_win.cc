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
  // Read the request from the browser to the agent
  if (!ReadRequest())
    return -1;
  // Prepare the response so that ALLOW verdicts are the default().
  return UpdateResponse(*response(),
      request()->tags_size() > 0 ? request()->tags(0) : std::string(),
      ContentAnalysisResponse::Result::SUCCESS);
}

int SessionWin::Close() {
  Shutdown();
  return SessionBase::Close();
}

void SessionWin::Shutdown() {
  if (hPipe_ != INVALID_HANDLE_VALUE) {
    FlushFileBuffers(hPipe_);
    CloseHandle(hPipe_);
    hPipe_ = INVALID_HANDLE_VALUE;
  }
}

// Send will write a response from the agent back to the browser
// And then read back an acknowledgement from the browser
// Returns 0 on success.
int SessionWin::Send() {
  // Write the response from the agent back to the browser
  if (!WriteResponse())
    return -1;
  // Second read the acknowledgement from the browser back to the agent
  if (!ReadAcknowledgement())
    return -1;
  return 0;
}

// Read the request from the browser to the agent
bool SessionWin::ReadRequest() {
  int final_size = 0;
  DWORD err = ERROR_SUCCESS;
  std::vector<char> buffer(kBufferSize);
  char* p = buffer.data();
  // Read from the pipe until the end of message.
  // Grows a buffer as needed to fit the whole request
  while (true) {
    DWORD read;
    // Read from the pipe into the buffer
    if (ReadFile(hPipe_, p, kBufferSize, &read, nullptr)) {
      // pipe is fully drained
      // read is the number of bytes drained
      final_size += read;
      break;
    } else {
      err = GetLastError();
      // if err is not MORE_DATA, there was an error while reading
      if (err != ERROR_MORE_DATA)
        break;
      // if err is MORE_DATA, resize buffer and continue reading
      buffer.resize(buffer.size() + kBufferSize);
      p = buffer.data() + buffer.size() - kBufferSize;
    }
  }
  // Check if any bytes were read
  if (final_size == 0)
    return false;
  // Parse the request
  // TODO(rogerta): do some basic validation of the request.
  return request()->ParseFromArray(buffer.data(), final_size);
}

// Receive an acknowledgement from the browser.
bool SessionWin::ReadAcknowledgement() {
  DWORD read;
  DWORD err = ERROR_SUCCESS;
  // NOTE: assumption is that acknowledgement is never larger
  // than 4096 (kBufferSize) bytes.
  std::vector<char> buffer(kBufferSize);
  if (!ReadFile(hPipe_, buffer.data(), buffer.size(), &read, nullptr)) {
    err = GetLastError();
  }
  if (err != ERROR_SUCCESS)
    return false;
  // Parse the acknowledgment
  return acknowledgement()->ParseFromString(buffer.data());
}

// Send the response from the agent to the browser
bool SessionWin::WriteResponse() {
  DWORD written;
  std::string str;
  // Serializes the response to a string
  if (!response()->SerializeToString(&str))
    return false;
  // Write the string to the pipe
  return WriteFile(hPipe_, str.data(), str.size(), &written,
                   nullptr);
}

}  // namespace sdk
}  // namespace content_analysis
