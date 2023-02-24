// Copyright 2023 The Chromium Authors.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "scoped_print_handle_mac.h"

namespace content_analysis {
namespace sdk {

ScopedPrintHandleMac::ScopedPrintHandleMac(
    const ContentAnalysisRequest::PrintData& print_data)
    : ScopedPrintHandleBase(print_data) {
  // TODO
}

ScopedPrintHandleMac::~ScopedPrintHandleMac() {
  // TODO
}

const char* ScopedPrintHandleMac::data() {
  // TODO
  return nullptr;
}

}  // namespace sdk
}  // namespace content_analysis
