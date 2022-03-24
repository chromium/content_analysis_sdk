// Copyright 2022 The Chromium Authors.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/base/public/gmock.h"
#include "testing/base/public/gunit.h"
#include "agent_win.h"
#include "session_win.h"

namespace content_analysis {
namespace sdk {

namespace {
using ::testing::status::IsOkAndHolds;

TEST(AgentWinTest, GetCorrectUri){
    EXPECT_THAT(AgentWin::AgentWin("uri"), IsOkAndHolds("uri"));
}


}  // namespace

}  // namespace commitments
}  // namespace security