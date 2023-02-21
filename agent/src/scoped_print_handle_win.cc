// Copyright 2023 The Chromium Authors.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "scoped_print_handle_win.h"

namespace content_analysis {
namespace sdk {

// static
std::unique_ptr<ContentAnalysisEvent::ScopedPrintHandle>
ContentAnalysisEvent::ScopedPrintHandle::Create(
    const ContentAnalysisRequest& request) {
  if (!request.has_print_data()) {
    return nullptr;
  }

  return std::make_unique<ScopedPrintHandleWin>(request.print_data());
}


ScopedPrintHandleWin::ScopedPrintHandleWin(
    const ContentAnalysisRequest::PrintData& print_data)
    : ScopedPrintHandleBase(print_data),
      handle_(reinterpret_cast<HANDLE>(print_data.handle())) {
  mapped_ = MapViewOfFile(handle_, FILE_MAP_READ, 0, 0, 0);
}

ScopedPrintHandleWin::~ScopedPrintHandleWin() {
  CloseHandle(handle_);
}

const char* ScopedPrintHandleWin::data() {
  if (!handle_ || handle_ == INVALID_HANDLE_VALUE || !mapped_) {
    return nullptr;
  }
  return reinterpret_cast<const char*>(mapped_);
}

}  // namespace sdk
}  // namespace content_analysis