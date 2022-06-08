// Copyright 2022 The Chromium Authors.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>
#include <vector>

#include <windows.h>
#include <sddl.h>

#include "common/utils_win.h"

#include "agent_win.h"
#include "event_win.h"

namespace content_analysis {
namespace sdk {

// The minimum number of pipe in listening mode.  This is greater than one to
// handle the case of multiple instance of Google Chrome browser starting
// at the same time.
const DWORD kMinNumListeningPipeInstances = 2;

// The minimum number of handles to wait on.  This is the minimum number
// of pipes in listening mode plus the stop event.
const DWORD kMinNumWaitHandles = kMinNumListeningPipeInstances + 1;

// The default size of the buffer used to hold messages received from
// Google Chrome.
const DWORD kBufferSize = 4096;

// static
std::unique_ptr<Agent> Agent::Create(
    Config config,
    std::unique_ptr<AgentEventHandler> handler) {
  return std::make_unique<AgentWin>(std::move(config), std::move(handler));
}

AgentWin::Connection::Connection(const std::string& pipename,
                                 AgentEventHandler* handler,
                                 bool is_first_pipe)
    : handler_(handler)  {
  memset(&overlapped_, 0, sizeof(overlapped_));
  // Create a manual reset event as specified for overlapped IO.
  // Use default security attriutes and no name since this event is not
  // shared with other processes.
  overlapped_.hEvent = CreateEvent(/*securityAttr=*/nullptr,
                                   /*manualReset=*/TRUE,
                                   /*initialState=*/FALSE,
                                   /*name=*/nullptr);
  ResetInternal(pipename, is_first_pipe);
}

AgentWin::Connection::~Connection() {
  Cleanup();

  if (handle_ != INVALID_HANDLE_VALUE) {
    CloseHandle(handle_);
  }

  // Invalid event handles are represented as null.
  if (overlapped_.hEvent) {
    CloseHandle(overlapped_.hEvent);
  }
}

DWORD AgentWin::Connection::Reset(const std::string& pipename) {
  return ResetInternal(pipename, false);
}

DWORD AgentWin::Connection::HandleEvent(HANDLE handle) {
  DWORD err = ERROR_SUCCESS;
  DWORD count;
  BOOL success = GetOverlappedResult(handle, &overlapped_, &count, FALSE);
  if (!is_connected_) {
    // This connection is currently listing for a new connection from a Google
    // Chrome browser.  If the result is a success, this means the browser has
    // connected as expected.  Otherwise an error occured so report it to the
    // caller.
    if (success) {
      // A Google Chrome browser connected to the agent.  Reset this
      // connection object to handle communication with the browser and then
      // tell the handler about it.

      is_connected_ = true;
      buffer_.resize(kBufferSize);

      err = BuildBrowserInfo();
      if (err == ERROR_SUCCESS) {
        handler_->OnBrowserConnected(browser_info_);
      }
    } else {
      err = GetLastError();
    }
  } else {
    // Some data has arrived from Google Chrome. This data is (part of) an
    // instance of the proto message `ChromeToAgent`.
    //
    // If the message is small it is received in by one call to ReadFile().
    // If the message is larger it is received in by multiple calls to
    // ReadFile().
    //
    // `success` is true if the data just read is the last bytes for a message.
    // Otherwise it is false.
    err = OnReadFile(success, count);
  }

  // If all data has been read, queue another read.
  if (err == ERROR_SUCCESS || err == ERROR_MORE_DATA) {
    err = QueueReadFile(err == ERROR_SUCCESS);
  }

  if (err != ERROR_SUCCESS && err != ERROR_IO_PENDING &&
      err != ERROR_MORE_DATA) {
    Cleanup();
  } else {
    // Don't propagate all the "success" error codes to the called to keep
    // this simpler.
    err = ERROR_SUCCESS;
  }

  return err;
}

DWORD AgentWin::Connection::CreatePipe(
    const std::string& name,
    bool is_first_pipe,
    HANDLE* handle) {
  DWORD err = ERROR_SUCCESS;

  DWORD mode = PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED;
  if (is_first_pipe) {
    mode |= FILE_FLAG_FIRST_PIPE_INSTANCE;
  }
  *handle = CreateNamedPipeA(name.c_str(), mode,
    PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT |
    PIPE_REJECT_REMOTE_CLIENTS, PIPE_UNLIMITED_INSTANCES, kBufferSize,
    kBufferSize, 0, /*securityAttr=*/nullptr);
  if (*handle == INVALID_HANDLE_VALUE) {
    err = GetLastError();
  }

  return err;
}

DWORD AgentWin::Connection::ConnectPipe() {
  // In overlapped mode, connecting to a named pipe always returns false.
  if (ConnectNamedPipe(handle_, &overlapped_)) {
    return GetLastError();
  }

  DWORD err = GetLastError();
  if (err == ERROR_IO_PENDING) {
    // Waiting for a Google Chrome Browser to connect.
    return ERROR_SUCCESS;
  } else if (err == ERROR_PIPE_CONNECTED) {
    // A Google Chrome browser is already connected.  Make sure event is in
    // signaled state in order to process the connection.
    if (SetEvent(overlapped_.hEvent)) {
      err = ERROR_SUCCESS;
    } else {
      err = GetLastError();
    }
  }

  return err;
}

DWORD AgentWin::Connection::ResetInternal(const std::string& pipename,
                                          bool is_first_pipe) {
  DWORD err = ERROR_SUCCESS;

  // If this is the not the first time, disconnect from any existing Google
  // Chrome browser.  Otherwise creater a new pipe.
  if (handle_ != INVALID_HANDLE_VALUE) {
    if (!DisconnectNamedPipe(handle_)) {
      err = GetLastError();
    }
  } else {
    err = CreatePipe(pipename, is_first_pipe, &handle_);
  }

  // Make sure event starts in reset state.
  if (err == ERROR_SUCCESS && !ResetEvent(overlapped_.hEvent)) {
    err = GetLastError();
  }

  if (err == ERROR_SUCCESS) {
    err = ConnectPipe();
  }

  if (err != ERROR_SUCCESS) {
    Cleanup();
    handle_ = INVALID_HANDLE_VALUE;
  }

  return err;
}

void AgentWin::Connection::Cleanup() {
  if (is_connected_ && handler_) {
    handler_->OnBrowserDisconnected(browser_info_);
  }

  is_connected_ = false;
  browser_info_ = BrowserInfo();
  buffer_.clear();
  cursor_ = nullptr;
  read_size_ = 0;
  final_size_ = 0;

  if (handle_ != INVALID_HANDLE_VALUE) {
    // Cancel all outstanding IO requests on this pipe by using a null for
    // overlapped.
    CancelIoEx(handle_, /*overlapped=*/nullptr);
  }

  // This function does not close `handle_` or the event in `overlapped` so
  // that the server can resuse the pipe with an new Google Chrome browser
  // instance.
}

DWORD AgentWin::Connection::QueueReadFile(bool reset_cursor) {
  if (reset_cursor) {
    cursor_ = buffer_.data();
    read_size_ = buffer_.size();
    final_size_ = 0;
  }

  // When this function is called there are the following possiblities:
  //
  // 1/ Data is already available and the buffer is filled in.  ReadFile()
  //    return TRUE and the event is set.
  // 2/ Data is not avaiable yet.  ReadFile() returns FALSE and the last error
  //    is ERROR_IO_PENDING(997) and the event is reset.
  // 3/ Some error occurred, like for example Google Chrome stops.  ReadFile()
  //    returns FALSE and the last error is something other than
  //    ERROR_IO_PENDING, for example ERROR_BROKEN_PIPE(109).  The event
  //    state is unchanged.
  DWORD err = ERROR_SUCCESS;
  DWORD count;
  if (!ReadFile(handle_, cursor_, read_size_, &count, &overlapped_)) {
    err = GetLastError();
  }

  return err;
}

DWORD AgentWin::Connection::OnReadFile(BOOL done_reading, DWORD count) {
  final_size_ += count;

  // If `done_reading` is TRUE, this means the full message has been read.
  // Call the appropriate handler method.
  if (done_reading) {
    return CallHandler();
  }

  // If success os false, there are two possibilities:
  //
  // 1/ The last error is ERROR_MORE_DATA(234).  This means there are more
  //     bytes to read before the request message is complete.  Resize the
  //     buffer and adjust the cursor.  The caller will queue up another read
  //     and wait.
  // 2/ Some error occured.  In this case return the error.

  DWORD err = GetLastError();
  if (err == ERROR_MORE_DATA) {
    read_size_ = kBufferSize;
    buffer_.resize(buffer_.size() + read_size_);
    cursor_ = buffer_.data() + buffer_.size() - read_size_;
  }

  return err;
}

DWORD AgentWin::Connection::CallHandler() {
  ChromeToAgent message;
  if (!message.ParseFromArray(buffer_.data(), final_size_)) {
    // Malformed message.
    return ERROR_INVALID_DATA;
  }

  DWORD err = ERROR_SUCCESS;

  if (message.has_request()) {
    // This is a request from Google Chrome to perform a content analysis
    // request.
    //
    // Move the request from `message` to the event to reduce the amount
    // of memory allocation/copying and also because the the handler takes
    // ownership of the event.
    auto event = std::make_unique<ContentAnalysisEventWin>(
        handle_, browser_info_, std::move(*message.mutable_request()));
    err = event->Init();
    if (err == ERROR_SUCCESS) {
      handler_->OnAnalysisRequested(std::move(event));
    }
  } else if (message.has_ack()) {
    // This is an ack from Google Chrome that it has received a content
    // analysis response from the agent.
    handler_->OnResponseAcknowledged(message.ack());
  } else {
    // Malformed message.
    err = ERROR_INVALID_DATA;
  }

  return err;
}

DWORD AgentWin::Connection::BuildBrowserInfo() {
  if (!GetNamedPipeClientProcessId(handle_, &browser_info_.pid)) {
    return GetLastError();
  }

  HANDLE hProc = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE,
      browser_info_.pid);
  if (hProc == nullptr) {
    return GetLastError();
  }
  
  DWORD err = ERROR_SUCCESS;
  char path[MAX_PATH];
  DWORD size = sizeof(path);
  DWORD length = QueryFullProcessImageNameA(hProc, /*flags=*/0, path, &size);
  if (length == 0) {
    err = GetLastError();
  }

  CloseHandle(hProc);

  browser_info_.binary_path = path;
  return err;
}

AgentWin::AgentWin(
    Config config,
    std::unique_ptr<AgentEventHandler> event_handler)
  : AgentBase(std::move(config), std::move(event_handler)) {
  if (handler() == nullptr) {
    return;
  }

  stop_event_ = CreateEvent(/*securityAttr=*/nullptr,
                            /*manualReset=*/TRUE,
                            /*initialState=*/FALSE,
                            /*name=*/nullptr);
  if (stop_event_ == nullptr) {
    return;
  }

  std::string pipename =
      internal::GetPipeName(configuration().name,
                            configuration().user_specific);
  if (pipename.empty())
    return;

  pipename_ = pipename;

  connections_.reserve(kMinNumListeningPipeInstances);
  for (int i = 0; i < kMinNumListeningPipeInstances; ++i) {
    connections_.emplace_back(
        std::make_unique<Connection>(pipename_, handler(), i == 0));
    if (!connections_.back()->IsValid()) {
      Shutdown();
      break;
    }
  }
}

AgentWin::~AgentWin() {
  Shutdown();
}

void AgentWin::HandleEvents() {
  std::vector<HANDLE> wait_handles;
  DWORD err = ERROR_SUCCESS;
  bool stopped = false;
  while (!stopped && err == ERROR_SUCCESS) {
    err = HandleOneEvent(wait_handles, &stopped);
  }
}

int AgentWin::Stop() {
  SetEvent(stop_event_);
  return AgentBase::Stop();
}

void AgentWin::GetHandles(std::vector<HANDLE>& wait_handles) const {
  // Reserve enough space in the handles vector to include the stop event plus
  // all connections.
  wait_handles.clear();
  wait_handles.reserve(1 + connections_.size());

  for (auto& state : connections_) {
    HANDLE wait_handle = state->GetWaitHandle();
    if (!wait_handle) {
      wait_handles.clear();
      break;
    }
    wait_handles.push_back(wait_handle);
  }

  // Push the stop event last so that connections_ index calculations in
  // HandleOneEvent() don't have to account for this handle.
  wait_handles.push_back(stop_event_);
}

DWORD AgentWin::HandleOneEventForTesting() {
  std::vector<HANDLE> wait_handles;
  bool stopped;
  return HandleOneEvent(wait_handles, &stopped);
}

DWORD AgentWin::HandleOneEvent(std::vector<HANDLE>& wait_handles, bool* stopped) {
  *stopped = false;

  // Wait on the specified handles for an event to occur.
  GetHandles(wait_handles);
  if (wait_handles.size() < kMinNumWaitHandles) {
    return ERROR_INVALID_HANDLE;
  }

  DWORD index = WaitForMultipleObjects(
    wait_handles.size(), wait_handles.data(),
    /*waitAll=*/FALSE, /*timeoutMs=*/INFINITE);
  if (index == WAIT_FAILED) {
    return GetLastError();
  }

  // If the index if signaled handle is beyond the end of the connections_
  // vector, then the stop event was signaled.
  index -= WAIT_OBJECT_0;
  if (index == wait_handles.size() - 1) {
    *stopped = true;
    return ERROR_SUCCESS;
  }

  auto& connection = connections_[index];
  bool was_listening = !connection->IsConnected();
  DWORD err = connection->HandleEvent(wait_handles[index]);
  if (err != ERROR_SUCCESS) {
    // If `connection` was not listening and there are more than
    // kNumPipeInstances pipes, delete this connection.  Otherwise
    // reset it so that it becomes a listener.
    if (!was_listening &&
      connections_.size() > kMinNumListeningPipeInstances) {
      connections_.erase(connections_.begin() + index);
    } else {
      err = connection->Reset(pipename_);
    }
  }

  // If `connection` was listening and is now connected, create a new
  // one so that there are always kNumPipeInstances listening.
  if (err == ERROR_SUCCESS && was_listening && connection->IsConnected()) {
    connections_.emplace_back(
      std::make_unique<Connection>(pipename_, handler(), false));
  }

  return ERROR_SUCCESS;
}

void AgentWin::Shutdown() {
  connections_.clear();
  pipename_.clear();
  if (stop_event_ != nullptr) {
    CloseHandle(stop_event_);
    stop_event_ = nullptr;
  }
}

}  // namespace sdk
}  // namespace content_analysis
