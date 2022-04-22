// Copyright 2022 The Chromium Authors.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "client_mac.h"

namespace content_analysis {
namespace sdk {

// static
std::unique_ptr<Client> Client::Create(const Uri& uri) {
  return std::make_unique<ClientMac>(uri);
}

ClientMac::ClientMac(const Uri& uri) : ClientBase(uri) {}

int ClientMac::Send(const ContentAnalysisRequest& request,
                    ContentAnalysisResponse* response) {
  return -1;
}

}  // namespace sdk
}  // namespace content_analysis