// Copyright 2022 The Chromium Authors.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_ANALYSIS_BROWSER_SRC_CLIENT_BASE_H_
#define CONTENT_ANALYSIS_BROWSER_SRC_CLIENT_BASE_H_

#include "content_analysis/sdk/analysis_client.h"

namespace content_analysis {
namespace sdk {

// Base Client class with code common to all platforms.
class ClientBase : public Client {
 public:
  // Client:
  const Uri& GetUri() const override;

 protected:
  ClientBase(const Uri& uri);

 private:
  Uri uri_;
};

}  // namespace sdk
}  // namespace content_analysis

#endif  // CONTENT_ANALYSIS_BROWSER_SRC_CLIENT_BASE_H_