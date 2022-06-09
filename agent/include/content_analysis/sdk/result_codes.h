// Copyright 2022 The Chromium Authors.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_ANALYSIS_AGENT_INCLUDE_CONTENT_ANALYSIS_SDK_RESULT_CODES_H_
#define CONTENT_ANALYSIS_AGENT_INCLUDE_CONTENT_ANALYSIS_SDK_RESULT_CODES_H_

#include <stdint.h>

namespace content_analysis {
namespace sdk {

// Result codes of methods and functions.
// Results codes with the msb set (i.e. negative numbers) are unrecoverable failures.
enum class ResultCode : uint32_t {
  // Operation completed successfully.
  OK = 0,

  //////////////////////////// Recoverable errors ////////////////////////////

  // Response is missing a result message.
  ERR_MISSING_RESULT,

  // A resonse has already been sent for this request.
  ERR_RESPONSE_ALRREADY_SENT,

  // The request is missing a request token.
  ERR_MISSING_REQUEST_TOKEN,

  // The agent is not proplerly initialized to handle events.
  ERR_AGENT_NOT_INITIALIZED,

  // The browser sent an incorrectly formatted message.
  ERR_INVALID_REQUEST_FROM_BROWSER,

  // IO incomplete, the operation is still pending.
  ERR_IO_PENDING,

  // There is more data to read before the entire message has been received.
  ERR_MORE_DATA,

  // Cannot get process Id of browser.
  ERR_CANNOT_GET_BROWSER_PID,

  // CAnnot open browser process to extract info.
  ERR_CANNOT_OPEN_BROWSER_PROCESS,

  // Cannot get the full path to the brower's main binary file.
  ERR_CANNOT_GET_BROWSER_BINARY_PATH,

  // An internal error has occured.
  ERR_UNEXPECTED,

  /////////////////////////// Unrecoverable errors ///////////////////////////

  ERR_FIRST_UNCOVERABLE_ERROR = 0x80000000,

  // Another process is already running as an agent on this computer.
  ERR_AGENT_ALREADY_EXISTS = ERR_FIRST_UNCOVERABLE_ERROR,

  // An agent handler was not specified when creating an agent.
  ERR_AGENT_EVENT_HANDLER_NOT_SPECIFIED,

  // Could not create event to signal the agent to stop.
  ERR_CANNOT_CREATE_AGENT_STOP_EVENT,

  // Invalid channel name specified in Agent::Config.
  ERR_INVALID_CHANNEL_NAME,

  // Could not create event to perform async IO with a client.
  ERR_CANNOT_CREATE_CHANNEL_IO_EVENT,
};

inline bool IsRecoverableError(ResultCode err) {
  return err >= ResultCode::ERR_FIRST_UNCOVERABLE_ERROR;
}

}  // namespace sdk
}  // namespace content_analysis

#endif  // CONTENT_ANALYSIS_AGENT_INCLUDE_CONTENT_ANALYSIS_SDK_RESULT_CODES_H_
