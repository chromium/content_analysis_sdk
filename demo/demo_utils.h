// Copyright 2023 The Chromium Authors.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_ANALYSIS_DEMO_DEMO_UTILS_H_
#define CONTENT_ANALYSIS_DEMO_DEMO_UTILS_H_

#include <string>

int WriteHandleContentToFile(void* handle,
                             size_t size,
                             const std::string& path);

#endif  // CONTENT_ANALYSIS_DEMO_DEMO_UTILS_H_
