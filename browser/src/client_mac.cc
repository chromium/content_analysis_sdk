// Copyright 2022 The Chromium Authors.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "client_mac.h"

namespace content_analysis {
namespace sdk {

// static
std::unique_ptr<Client> Client::Create(Config config) {
  auto client = std::make_unique<ClientMac>(std::move(config));
  return client->IsConnected() ? std::move(client) : nullptr;
}

ClientMac::ClientMac(Config config) : ClientPosix(std::move(config)) {}

}  // namespace sdk
}  // namespace content_analysis
