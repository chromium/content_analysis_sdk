// Copyright 2022 The Chromium Authors.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "client_base.h"

namespace content_analysis {
namespace sdk {

ClientBase::ClientBase(const Uri& uri) : uri_(uri) {}

const Client::Uri& ClientBase::GetUri() const {
  return uri_;
}

bool IsSuccessfulHandshake(const ContentAnalysisRequest& request,
                           const ContentAnalysisResponse& response) {
  return
    request.has_handshake() &&
    response.has_handshake() &&
      (request.handshake().status() ==
        response.handshake().status() ==
        Handshake::Status::Handshake_Status_SUCCESS);
}

}  // namespace sdk
}  // namespace content_analysis
