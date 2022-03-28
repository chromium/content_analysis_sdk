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

  // Send the request from browser to agent
  bool ack_failure = false;
  DWORD written;
  if (WriteFile(handle, request_str.data(), request_str.size(), &written,
                nullptr)) {
    // Receive the response from agent to browser
    // NOTE: assumption is that response is never larger than this.
    std::vector<char> buffer(4096);
    DWORD read;
    if (ReadFile(handle, buffer.data(), buffer.size(), &read, nullptr)) {

      // Prepare an acknowledgement back from browser to agent
      Acknowledgement ack;
      if (response->ParseFromString(buffer.data())) {
        // If the response was successfully parsed
        // return status SUCCESS
        ack.set_status(Acknowledgement_Status_SUCCESS);
      } else {
        // If the response was not successfully parsed
        // return status FAILURE
        ack.set_status(Acknowledgement_Status_FAILURE);
        ack_failure = true;
      }
      std::string ack_str;
      ack.SerializeToString(&ack_str);
      DWORD ack_written;
      // Send the acknowledgement down the pipe
      if (!WriteFile(handle, ack_str.data(), ack_str.size(), &ack_written, nullptr)) {
        err = GetLastError();
      }
    } else {
      err = GetLastError();
    }
  }

  CloseHandle(handle);
  return err == ERROR_SUCCESS && !ack_failure ? 0 : -1;
}

}  // namespace sdk
}  // namespace content_analysis