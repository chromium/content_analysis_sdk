// Copyright 2023 The Chromium Authors.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "scoped_print_handle_posix.h"

namespace content_analysis {
namespace sdk {



ScopedPrintHandlePosix::ScopedPrintHandlePosix(
    const ContentAnalysisRequest::PrintData& print_data)
    : ScopedPrintHandleBase(print_data) {
  // TODO
}

ScopedPrintHandlePosix::~ScopedPrintHandlePosix() {
  // TODO
}

const char* ScopedPrintHandlePosix::data() {
  // TODO
  return nullptr;
}

}  // namespace sdk
}  // namespace content_analysis
