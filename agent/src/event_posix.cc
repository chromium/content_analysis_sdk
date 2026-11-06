// Copyright 2022 The Chromium Authors.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "event_posix.h"

#include <errno.h>

#include <ios>
#include <sstream>
#include <utility>

#include "common/utils_posix.h"

namespace content_analysis {
namespace sdk {

ContentAnalysisEventPosix::ContentAnalysisEventPosix(
    const BrowserInfo& browser_info,
    ContentAnalysisRequest req)
    : ContentAnalysisEventPosix(nullptr, browser_info, std::move(req)) {}

ContentAnalysisEventPosix::ContentAnalysisEventPosix(
    std::shared_ptr<ConnectionChannel> channel,
    const BrowserInfo& browser_info,
    ContentAnalysisRequest req)
    : ContentAnalysisEventBase(browser_info), channel_(std::move(channel)) {
  *request() = std::move(req);
}

ContentAnalysisEventPosix::~ContentAnalysisEventPosix() {
  Shutdown();
}

ResultCode ContentAnalysisEventPosix::Init() {
  if (request()->request_token().empty()) {
    return ResultCode::ERR_MISSING_REQUEST_TOKEN;
  }

  response()->set_request_token(request()->request_token());

  // Prepare the response so that ALLOW verdicts are the default.
  return UpdateResponse(*response(),
      request()->tags_size() > 0 ? request()->tags(0) : std::string(),
      ContentAnalysisResponse::Result::SUCCESS);
}

ResultCode ContentAnalysisEventPosix::Close() {
  Shutdown();
  return ContentAnalysisEventBase::Close();
}

ResultCode ContentAnalysisEventPosix::Send() {
  if (response_sent_) {
    return ResultCode::ERR_RESPONSE_ALREADY_SENT;
  }
  response_sent_ = true;

  if (!channel_) {
    return ResultCode::ERR_BROKEN_PIPE;
  }

  std::string bytes;
  if (!agent_to_chrome()->SerializeToString(&bytes)) {
    return ResultCode::ERR_UNEXPECTED;
  }

  std::lock_guard<std::mutex> lock(channel_->write_mutex);
  if (channel_->fd < 0) {
    return ResultCode::ERR_BROKEN_PIPE;
  }
  int err = internal::WriteFramedMessage(channel_->fd, bytes);
  if (err == 0) {
    return ResultCode::OK;
  }
  return err == ECONNRESET ? ResultCode::ERR_BROKEN_PIPE
                           : ResultCode::ERR_UNEXPECTED;
}

std::string ContentAnalysisEventPosix::DebugString() const {
  std::stringstream state;
  state.setf(std::ios::boolalpha);
  state << "ContentAnalysisEventPosix{fd=" << (channel_ ? channel_->fd : -1);
  state << " pid=" << GetBrowserInfo().pid;
  state << " rtoken=" << GetRequest().request_token();
  state << " sent=" << response_sent_;
  state << "}";
  return state.str();
}

void ContentAnalysisEventPosix::Shutdown() {
  // If no response has been sent yet, send the default one now so the
  // browser is not left waiting; after shutdown there is no other chance.
  if (channel_ && !response_sent_) {
    Send();
  }
  channel_.reset();
}

}  // namespace sdk
}  // namespace content_analysis
