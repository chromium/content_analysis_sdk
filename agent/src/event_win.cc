// Copyright 2022 The Chromium Authors.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "event_win.h"

namespace content_analysis {
namespace sdk {

// Writes a string to the pipe. Returns True if successful, else returns False.
static DWORD WriteMessageToPipe(HANDLE pipe, const std::string& message) {
  if (message.empty()) {
    return ERROR_SUCCESS;
  }

  OVERLAPPED overlapped;
  memset(&overlapped, 0, sizeof(overlapped));
  overlapped.hEvent = CreateEvent(nullptr, TRUE, FALSE, nullptr);
  if (overlapped.hEvent == nullptr) {
    return GetLastError();
  }

  DWORD err = ERROR_SUCCESS;
  const char* cursor = message.data();
  for (DWORD size = message.length(); size > 0;) {
    if (WriteFile(pipe, cursor, size, nullptr, &overlapped)) {
      break;
    }

    DWORD written;
    if (!GetOverlappedResult(pipe, &overlapped, &written, TRUE)) {
      err = GetLastError();
      break;
    }

    cursor += written;
    size -= written;
  }

  CloseHandle(overlapped.hEvent);
  return err;
}


ContentAnalysisEventWin::ContentAnalysisEventWin(HANDLE handle,
                                                 const BrowserInfo& browser_info,
                                                 ContentAnalysisRequest req)
    : ContentAnalysisEventBase(browser_info),
      hPipe_(handle) {
  // TODO(rogerta): do some basic validation of the request.
  *request() = std::move(req);
}

ContentAnalysisEventWin::~ContentAnalysisEventWin() {
  Shutdown();
}

DWORD ContentAnalysisEventWin::Init() {
  // Prepare the response so that ALLOW verdicts are the default().
  UpdateResponse(*response(),
      request()->tags_size() > 0 ? request()->tags(0) : std::string(),
      ContentAnalysisResponse::Result::SUCCESS);
  return ERROR_SUCCESS;
}

int ContentAnalysisEventWin::Close() {
  Shutdown();
  return ContentAnalysisEventBase::Close();
}

int ContentAnalysisEventWin::Send() {
  DWORD err = WriteMessageToPipe(hPipe_,
                                 agent_to_chrome()->SerializeAsString());
  return err == ERROR_SUCCESS ? 0 : -1;
}

void ContentAnalysisEventWin::Shutdown() {
  if (hPipe_ != INVALID_HANDLE_VALUE) {
    // This event does not own the pipe, so don't close it.
    FlushFileBuffers(hPipe_);
    hPipe_ = INVALID_HANDLE_VALUE;
  }
}

}  // namespace sdk
}  // namespace content_analysis
