// Copyright 2022 The Chromium Authors.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <windows.h>

#include <vector>

#include "client_win.h"

namespace content_analysis {
namespace sdk {

// static
std::unique_ptr<Client> Client::Create(const Uri& uri) {
  return std::make_unique<ClientWin>(uri);
}

ClientWin::ClientWin(const Uri& uri) : ClientBase(uri) {
  pipename_ = "\\\\.\\pipe\\" + uri;
}

DWORD ClientWin::ConnectToPipe(HANDLE* handle) {
  HANDLE h = INVALID_HANDLE_VALUE;
  while (h == INVALID_HANDLE_VALUE) {
    h = CreateFile(pipename_.c_str(), GENERIC_READ | GENERIC_WRITE, 0,
        nullptr, OPEN_EXISTING, 0, nullptr);
    if (h == INVALID_HANDLE_VALUE) {
      if (GetLastError() != ERROR_PIPE_BUSY) {
        break;
      }

      if (!WaitNamedPipe(pipename_.c_str(), NMPWAIT_USE_DEFAULT_WAIT)) {
        break;
      }
    }
  }

  if (h == INVALID_HANDLE_VALUE) {
    return GetLastError();
  }

  // Change to message read mode to match server side.
  DWORD mode = PIPE_READMODE_MESSAGE;
  if (!SetNamedPipeHandleState(h, &mode, nullptr, nullptr)) {
    CloseHandle(h);
    return GetLastError();
  }

  *handle = h;
  return ERROR_SUCCESS;
}

int ClientWin::Send(const ContentAnalysisRequest& request,
                    ContentAnalysisResponse* response) {
  std::string request_str;
  if (!request.SerializeToString(&request_str)) {
    return -1;
  }

  HANDLE handle;
  DWORD err = ConnectToPipe(&handle);
  if (err != ERROR_SUCCESS) {
    return -1;
  }

  DWORD written;
  if (WriteFile(handle, request_str.data(), request_str.size(), &written,
                nullptr)) {
    // NOTE: assumption is that response is never larger than this.
    std::vector<char> buffer(4096);
    DWORD read;
    if (ReadFile(handle, buffer.data(), buffer.size(), &read, nullptr)) {
      response->ParseFromString(buffer.data());
    } else {
      err = GetLastError();
    }
  }

  CloseHandle(handle);
  return err == ERROR_SUCCESS ? 0 : -1;
}

}  // namespace sdk
}  // namespace content_analysis