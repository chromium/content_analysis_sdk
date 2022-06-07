// Copyright 2022 The Chromium Authors.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "agent/src/event_win.h"
#include "content_analysis/sdk/analysis_agent.h"
#include "gtest/gtest.h"

namespace content_analysis {
namespace sdk {
namespace testing {

std::unique_ptr<ContentAnalysisEventWin> CreateEvent(
    const BrowserInfo& browser_info,
    ContentAnalysisRequest request) {
  return std::make_unique<ContentAnalysisEventWin>(
      INVALID_HANDLE_VALUE, browser_info, request);
}

TEST(EventTest, Create_BrowserInfo) {
  const BrowserInfo bi{12345, "/path/to/binary"};
  ContentAnalysisRequest request;
  *request.add_tags() = "foo";
  request.set_request_token("req-token");

  auto event = CreateEvent(bi, request);
  ASSERT_TRUE(event);

  ASSERT_EQ(bi.pid, event->GetBrowserInfo().pid);
  ASSERT_EQ(bi.binary_path, event->GetBrowserInfo().binary_path);
}

TEST(EventTest, Create_Request) {
  const BrowserInfo bi{ 12345, "/path/to/binary" };
  ContentAnalysisRequest request;
  *request.add_tags() = "foo";
  request.set_request_token("req-token");

  auto event = CreateEvent(bi, request);
  ASSERT_TRUE(event);

  ASSERT_EQ(1u, event->GetRequest().tags_size());
  ASSERT_EQ("foo", event->GetRequest().tags(0));
  ASSERT_TRUE(event->GetRequest().has_request_token());
  ASSERT_EQ("req-token", event->GetRequest().request_token());
}

TEST(EventTest, Create_Init) {
  const BrowserInfo bi{ 12345, "/path/to/binary" };
  ContentAnalysisRequest request;
  *request.add_tags() = "foo";
  request.set_request_token("req-token");

  auto event = CreateEvent(bi, request);
  ASSERT_TRUE(event);

  ASSERT_EQ(ERROR_SUCCESS, event->Init());

  // Initializing an event should initialize the contained response for a
  // success verdict that matches the request.
  ASSERT_EQ(request.request_token(), event->GetResponse().request_token());
  ASSERT_EQ(1u, event->GetResponse().results_size());
  ASSERT_EQ(ContentAnalysisResponse::Result::SUCCESS,
            event->GetResponse().results(0).status());
  ASSERT_TRUE(event->GetResponse().results(0).has_tag());
  ASSERT_EQ("foo", event->GetResponse().results(0).tag());
  ASSERT_EQ(0u, event->GetResponse().results(0).triggered_rules_size());
}

}  // namespace testing
}  // namespace sdk
}  // namespace content_analysis

