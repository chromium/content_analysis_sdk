// Copyright 2022 The Chromium Authors.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <thread>

#include "agent/src/agent_win.h"
#include "agent/src/event_win.h"
#include "browser/src/client_win.h"
#include "gtest/gtest.h"

namespace content_analysis {
namespace sdk {
namespace testing {

struct TestHandler : public AgentEventHandler {
  void OnBrowserConnected(const BrowserInfo& info) override {
    last_info_ = info;
    ++connect_count_;
  }
  void OnBrowserDisconnected(const BrowserInfo& info) override {
    last_info_ = info;
    ++disconnect_count_;
  }
  void OnAnalysisRequested(
      std::unique_ptr<ContentAnalysisEvent> event) override {
    ++request_count_;
    int ret = event->Send();
    ASSERT_EQ(0, ret);
  }
  void OnResponseAcknowledged(
      const ContentAnalysisAcknowledgement& ack) override {
    ++ack_count_;
  }

  int connect_count_ = 0;
  int disconnect_count_ = 0;
  int request_count_ = 0;
  int ack_count_ = 0;
  BrowserInfo last_info_;
};

struct CloseEventTestHandler : public TestHandler {
  void OnAnalysisRequested(
    std::unique_ptr<ContentAnalysisEvent> event) override {
    ++request_count_;

    // Closing the event before sending should generate an error.
    int ret = event->Close();
    ASSERT_EQ(0, ret);

    ret = event->Send();
    ASSERT_NE(0, ret);
  }
};

struct DoubleSendTestHandler : public TestHandler {
  void OnAnalysisRequested(
      std::unique_ptr<ContentAnalysisEvent> event) override {
    ++request_count_;

    int ret = event->Send();
    ASSERT_EQ(0, ret);

    // Trying to send again fails.
    ret = event->Send();
    ASSERT_NE(0, ret);
  }
};

std::unique_ptr<AgentWin> CreateAgent(
    Agent::Config config,
  TestHandler** handler_ptr) {
  auto handler = std::make_unique<TestHandler>();
  *handler_ptr = handler.get();
  return std::make_unique<AgentWin>(
      std::move(config), std::move(handler));
}

std::unique_ptr<ClientWin> CreateClient(
  Client::Config config) {
  return std::make_unique<ClientWin>(std::move(config));
}

TEST(AgentTest, Create) {
  const Agent::Config config{"test", false};
  TestHandler* handler_ptr;
  auto agent = CreateAgent(config, &handler_ptr);
  ASSERT_TRUE(agent);
  ASSERT_TRUE(handler_ptr);

  ASSERT_EQ(config.name, agent->GetConfig().name);
  ASSERT_EQ(config.user_specific, agent->GetConfig().user_specific);
}

TEST(AgentTest, Create_InvalidPipename) {
  // TODO(rogerta): The win32 docs say that a backslash is an invalid
  // character for a pipename, but it seemed to work correctly in testing.
  // Using an empty name was the easiest way to generate an invalid pipe
  // name.
  const Agent::Config config{"", false};
  TestHandler* handler_ptr;
  auto agent = CreateAgent(config, &handler_ptr);
  ASSERT_TRUE(agent);

  ASSERT_NE(ERROR_SUCCESS, agent->HandleOneEventForTesting());
}

// Can't create two agents with the same name.
TEST(AgentTest, Create_SecondFails) {
  const Agent::Config config{ "test", false };
  TestHandler* handler_ptr1;
  auto agent1 = CreateAgent(config, &handler_ptr1);
  ASSERT_TRUE(agent1);

  TestHandler* handler_ptr2;
  auto agent2 = CreateAgent(config, &handler_ptr2);
  ASSERT_TRUE(agent2);

  ASSERT_NE(ERROR_SUCCESS, agent2->HandleOneEventForTesting());
}

TEST(AgentTest, Close) {
  TestHandler* handler_ptr;
  auto agent = CreateAgent({ "test", false }, &handler_ptr);
  ASSERT_TRUE(agent);

  // Create a separate thread to stop the agent.
  std::thread thread([&agent]() {
    agent->Stop();
  });

  agent->HandleEvents();
  thread.join();
}

TEST(AgentTest, ConnectAndClose) {
  const Agent::Config aconfig{ "test", false };
  const Client::Config cconfig{ "test", false };

  // Create an agent and client that connects to it.
  TestHandler* handler_ptr;
  auto agent = CreateAgent(aconfig, &handler_ptr);
  ASSERT_TRUE(agent);
  auto client = CreateClient(cconfig);
  ASSERT_TRUE(client);
  ASSERT_EQ(cconfig.name, client->GetConfig().name);
  ASSERT_EQ(cconfig.user_specific, client->GetConfig().user_specific);

  agent->HandleOneEventForTesting();
  ASSERT_EQ(1, handler_ptr->connect_count_);
  ASSERT_EQ(0, handler_ptr->disconnect_count_);
  ASSERT_EQ(GetCurrentProcessId(), handler_ptr->last_info_.pid);

  // Close the client and make sure a disconnect is received.
  client.reset();
  agent->HandleOneEventForTesting();
  ASSERT_EQ(1, handler_ptr->connect_count_);
  ASSERT_EQ(1, handler_ptr->disconnect_count_);
  ASSERT_EQ(GetCurrentProcessId(), handler_ptr->last_info_.pid);
}

TEST(AgentTest, Request) {
  // Create an agent and client that connects to it.
  TestHandler* handler_ptr;
  auto agent = CreateAgent({"test", false}, &handler_ptr);
  ASSERT_TRUE(agent);

  bool done = false;

  // Create a thread to handle the client.  Since the client is sync, it can't
  // run in the same thread as the agent.
  std::thread client_thread([&done]() {
    auto client = CreateClient({"test", false});
    ASSERT_TRUE(client);

    // Send a request and wait for a response.
    ContentAnalysisRequest request;
    ContentAnalysisResponse response;
    request.set_request_token("req-token");
    *request.add_tags() = "dlp";
    int ret = client->Send(request, &response);
    ASSERT_EQ(0, ret);
    ASSERT_EQ(request.request_token(), response.request_token());

    done = true;
  });

  while (!done) {
    agent->HandleOneEventForTesting();
  }
  ASSERT_EQ(1, handler_ptr->request_count_);

  client_thread.join();
}

TEST(AgentTest, Request_DoubleSend) {
  auto handler = std::make_unique<DoubleSendTestHandler>();
  DoubleSendTestHandler* handler_ptr = handler.get();
  auto agent = std::make_unique<AgentWin>(
    Agent::Config{"test", false}, std::move(handler));
  ASSERT_TRUE(agent);

  bool done = false;

  // Create a thread to handle the client.  Since the client is sync, it can't
  // run in the same thread as the agent.
  std::thread client_thread([&done]() {
    auto client = CreateClient({ "test", false });
    ASSERT_TRUE(client);

    // Send a request and wait for a response.
    ContentAnalysisRequest request;
    ContentAnalysisResponse response;
    request.set_request_token("req-token");
    *request.add_tags() = "dlp";
    int ret = client->Send(request, &response);
    ASSERT_EQ(0, ret);
    ASSERT_EQ(request.request_token(), response.request_token());

    done = true;
    });

  while (!done) {
    agent->HandleOneEventForTesting();
  }
  ASSERT_EQ(1, handler_ptr->request_count_);

  client_thread.join();
}

TEST(AgentTest, Request_CloseEvent) {
  auto handler = std::make_unique<CloseEventTestHandler>();
  CloseEventTestHandler* handler_ptr = handler.get();
  auto agent = std::make_unique<AgentWin>(
      Agent::Config{"test", false}, std::move(handler));
  ASSERT_TRUE(agent);

  bool done = false;

  // Create a thread to handle the client.  Since the client is sync, it can't
  // run in the same thread as the agent.
  std::thread client_thread([&done]() {
    auto client = CreateClient({"test", false});
    ASSERT_TRUE(client);

    // Send a request and wait for a response.
    ContentAnalysisRequest request;
    ContentAnalysisResponse response;
    request.set_request_token("req-token");
    *request.add_tags() = "dlp";
    int ret = client->Send(request, &response);
    ASSERT_EQ(0, ret);
    ASSERT_EQ(request.request_token(), response.request_token());

    done = true;
  });

  while (!done) {
    agent->HandleOneEventForTesting();
  }
  ASSERT_EQ(1, handler_ptr->request_count_);

  client_thread.join();
}

TEST(AgentTest, Ack) {
  // Create an agent and client that connects to it.
  TestHandler* handler_ptr;
  auto agent = CreateAgent({ "test", false }, &handler_ptr);
  ASSERT_TRUE(agent);

  bool done = false;

  // Create a thread to handle the client.  Since the client is sync, it can't
  // run in the same thread as the agent.
  std::thread client_thread([&done]() {
    auto client = CreateClient({"test", false});
    ASSERT_TRUE(client);

    // Send a request and wait for a response.
    ContentAnalysisRequest request;
    ContentAnalysisResponse response;
    request.set_request_token("req-token");
    *request.add_tags() = "dlp";
    int ret = client->Send(request, &response);
    ASSERT_EQ(0, ret);

    ContentAnalysisAcknowledgement ack;
    ack.set_request_token(request.request_token());
    ret = client->Acknowledge(ack);
    ASSERT_EQ(0, ret);

    done = true;
  });

  while (!done) {
    agent->HandleOneEventForTesting();
  }
  ASSERT_EQ(1, handler_ptr->request_count_);
  ASSERT_EQ(1, handler_ptr->ack_count_);

  client_thread.join();
}

}  // namespace testing
}  // namespace sdk
}  // namespace content_analysis

