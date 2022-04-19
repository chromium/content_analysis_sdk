// Copyright 2022 The Chromium Authors.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <windows.h>

#include <vector>

#include "client_win.h"

namespace content_analysis {
namespace sdk {

const DWORD kBufferSize = 4096;

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
  HANDLE handle;
  if (ConnectToPipe(&handle) != ERROR_SUCCESS) {
    return -1;
  }

  Handshake handshake;
  Acknowledgement acknowledgement;
  std::vector<char> buffer;

  bool handshake_sent = false;
  bool request_sent = false;
  bool acknowledgement_sent = false;
  bool response_received = false;
  bool success = false;

  handshake.set_content_analysis_requested(request.has_content_data());
  
  handshake_sent = WriteMessageToPipe(handle, handshake.SerializeAsString());

  if (handshake_sent && request.has_content_data())
    request_sent = WriteMessageToPipe(handle, request.SerializeAsString());

  if (request_sent) {
    buffer = ReadNextMessageFromPipe(handle);
    response_received = response->ParseFromArray(buffer.data(), buffer.size());
  }

  if (response_received) {
    acknowledgement.set_verdict_received(!response->results().empty());
    acknowledgement_sent = 
      WriteMessageToPipe(handle, acknowledgement.SerializeAsString());
  }

  CloseHandle(handle);

  success = request.has_content_data() ? acknowledgement_sent : handshake_sent;
  return success ? 0 : -1;
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

// Writes a string to the pipe. Returns True if successful, else returns False.
bool WriteMessageToPipe(HANDLE pipe, const std::string& message) {
  if (message.length() > 0) {
    DWORD written;
    return WriteFile(pipe, message.data(), message.size(), &written,
                  nullptr);
  }
  return false;
}

}  // namespace sdk
}  // namespace content_analysis