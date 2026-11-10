// Copyright 2022 The Chromium Authors.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_ANALYSIS_BROWSER_SRC_CLIENT_MAC_H_
#define CONTENT_ANALYSIS_BROWSER_SRC_CLIENT_MAC_H_

#include "client_posix.h"

namespace content_analysis {
namespace sdk {

// Client implementation for macOS.  macOS uses the same Unix domain socket
// transport as Linux; only the per-user socket directory differs, and that
// is handled in common/utils_posix.cc.
class ClientMac : public ClientPosix {
 public:
  explicit ClientMac(Config config);
};

}  // namespace sdk
}  // namespace content_analysis

#endif  // CONTENT_ANALYSIS_BROWSER_SRC_CLIENT_MAC_H_
