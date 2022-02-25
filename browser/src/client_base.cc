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

}  // namespace sdk
}  // namespace content_analysis
