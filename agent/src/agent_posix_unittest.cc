// Copyright 2026 The Chromium Authors.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <errno.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <unistd.h>

#include <atomic>
#include <chrono>
#include <cstring>
#include <memory>
#include <mutex>
#include <string>
#include <thread>

#include "agent/src/agent_posix.h"
#include "agent/src/event_posix.h"
#include "browser/src/client_posix.h"
#include "common/utils_posix.h"
#include "content_analysis/sdk/analysis_agent.h"
#include "gtest/gtest.h"

namespace content_analysis {
namespace sdk {
namespace testing {

namespace {

// Waits up to a few seconds for `predicate` to become true.
template <typename Predicate>
bool WaitFor(Predicate predicate) {
  auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
  while (!predicate()) {
    if (std::chrono::steady_clock::now() > deadline) {
      return false;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
  }
  return true;
}

// A private directory for the test socket.  Absolute channel names are
// used verbatim, which keeps the tests independent of /var/run and of the
// user's runtime directory.
class SocketDirectory {
 public:
  SocketDirectory() {
    char pattern[] = "/tmp/cas-test-XXXXXX";
    const char* created = mkdtemp(pattern);
    path_ = created ? created : "";
  }
  ~SocketDirectory() {
    if (!path_.empty()) {
      unlink((path_ + "/agent.sock").c_str());
      rmdir(path_.c_str());
    }
  }
  std::string SocketPath() const { return path_ + "/agent.sock"; }

 private:
  std::string path_;
};

struct TestHandler : public AgentEventHandler {
  void OnBrowserConnected(const BrowserInfo& info) override {
    std::lock_guard<std::mutex> lock(mutex);
    last_info = info;
    ++connect_count;
  }
  void OnBrowserDisconnected(const BrowserInfo& info) override {
    std::lock_guard<std::mutex> lock(mutex);
    last_info = info;
    ++disconnect_count;
  }
  void OnAnalysisRequested(
      std::unique_ptr<ContentAnalysisEvent> event) override {
    ++request_count;
    if (block_everything) {
      SetEventVerdictToBlock(event.get());
    }
    {
      std::lock_guard<std::mutex> lock(mutex);
      last_text_size = event->GetRequest().text_content().size();
    }
    ResultCode rc = event->Send();
    EXPECT_EQ(ResultCode::OK, rc);
    // Sending twice is an error.
    EXPECT_EQ(ResultCode::ERR_RESPONSE_ALREADY_SENT, event->Send());
  }
  void OnResponseAcknowledged(
      const ContentAnalysisAcknowledgement& ack) override {
    ++ack_count;
  }
  void OnCancelRequests(
      const ContentAnalysisCancelRequests& cancel) override {
    ++cancel_count;
  }
  void OnInternalError(const char* context, ResultCode error) override {
    ++error_count;
  }

  std::mutex mutex;
  bool block_everything = false;
  std::atomic<int> connect_count{0};
  std::atomic<int> disconnect_count{0};
  std::atomic<int> request_count{0};
  std::atomic<int> ack_count{0};
  std::atomic<int> cancel_count{0};
  std::atomic<int> error_count{0};
  size_t last_text_size = 0;
  BrowserInfo last_info;
};

// Runs an agent on a background thread for the lifetime of the object.
class RunningAgent {
 public:
  RunningAgent(const std::string& path, TestHandler** handler_out,
               bool block_everything = false) {
    auto handler = std::make_unique<TestHandler>();
    handler->block_everything = block_everything;
    handler_ = handler.get();
    *handler_out = handler_;
    agent_ = Agent::Create({path, false}, std::move(handler), &rc_);
    if (agent_) {
      thread_ = std::thread([this] { loop_rc_ = agent_->HandleEvents(); });
    }
  }
  ~RunningAgent() {
    if (agent_) {
      agent_->Stop();
      thread_.join();
    }
  }

  ResultCode create_result() const { return rc_; }
  ResultCode loop_result() const { return loop_rc_; }
  TestHandler* handler() const { return handler_; }
  Agent* agent() const { return agent_.get(); }

 private:
  ResultCode rc_ = ResultCode::ERR_UNEXPECTED;
  ResultCode loop_rc_ = ResultCode::ERR_UNEXPECTED;
  TestHandler* handler_ = nullptr;
  std::unique_ptr<Agent> agent_;
  std::thread thread_;
};

ContentAnalysisRequest MakeRequest(const std::string& token,
                                   const std::string& text) {
  ContentAnalysisRequest request;
  request.set_request_token(token);
  *request.add_tags() = "dlp";
  request.set_analysis_connector(BULK_DATA_ENTRY);
  request.set_text_content(text);
  return request;
}

}  // namespace

TEST(UtilsPosixTest, ChannelNames) {
  EXPECT_TRUE(internal::IsValidChannelName("brcm_chrm_cas"));
  EXPECT_TRUE(internal::IsValidChannelName("path_user"));
  EXPECT_TRUE(internal::IsValidChannelName("/tmp/x.sock"));
  EXPECT_FALSE(internal::IsValidChannelName(""));
  EXPECT_FALSE(internal::IsValidChannelName(".hidden"));
  EXPECT_FALSE(internal::IsValidChannelName("../escape"));
  EXPECT_FALSE(internal::IsValidChannelName("a/b"));
  EXPECT_FALSE(internal::IsValidChannelName("with space"));
  EXPECT_FALSE(internal::IsValidChannelName(std::string(65, 'a')));

  EXPECT_EQ("/var/run/brcm_chrm_cas.sock",
            internal::GetSocketPath("brcm_chrm_cas", false));
  EXPECT_EQ("/tmp/x.sock", internal::GetSocketPath("/tmp/x.sock", true));
  EXPECT_EQ("", internal::GetSocketPath("../escape", false));

  std::string user_path = internal::GetSocketPath("path_user", true);
  if (!user_path.empty()) {
    EXPECT_EQ('/', user_path[0]);
    EXPECT_NE(std::string::npos, user_path.find("path_user.sock"));
    EXPECT_EQ(std::string::npos, user_path.find("/var/run/"));
  }
}

TEST(UtilsPosixTest, FrameHeaderRoundTrip) {
  char header[internal::kFrameHeaderSize];
  for (uint32_t value : {1u, 255u, 4096u, 0x01020304u, 0xfffffffeu}) {
    internal::EncodeFrameHeader(value, header);
    EXPECT_EQ(value, internal::DecodeFrameHeader(header));
  }
}

TEST(UtilsPosixTest, FramedMessagesOverSocketPair) {
  int fds[2];
  ASSERT_EQ(0, socketpair(AF_UNIX, SOCK_STREAM, 0, fds));

  // The payload exceeds the kernel socket buffer, so the writer must run
  // concurrently with the reader.
  std::string payload(100000, 'x');
  int write_rc = -1;
  std::thread writer([&] {
    write_rc = internal::WriteFramedMessage(fds[0], payload);
  });
  std::string received;
  ASSERT_EQ(0, internal::ReadFramedMessage(fds[1], &received));
  writer.join();
  EXPECT_EQ(0, write_rc);
  EXPECT_EQ(payload, received);

  EXPECT_EQ(EMSGSIZE, internal::WriteFramedMessage(fds[0], ""));

  // An oversized length prefix is rejected before any body is read.
  char header[internal::kFrameHeaderSize];
  internal::EncodeFrameHeader(internal::kMaxMessageSize + 1, header);
  ASSERT_EQ(0, internal::WriteAll(fds[0], header, sizeof(header)));
  EXPECT_EQ(EMSGSIZE, internal::ReadFramedMessage(fds[1], &received));

  close(fds[0]);
  EXPECT_EQ(ECONNRESET, internal::ReadFramedMessage(fds[1], &received));
  close(fds[1]);
}

TEST(AgentPosixTest, CreateRequiresHandler) {
  SocketDirectory directory;
  ResultCode rc;
  auto agent = Agent::Create({directory.SocketPath(), false}, nullptr, &rc);
  EXPECT_FALSE(agent);
  EXPECT_EQ(ResultCode::ERR_AGENT_EVENT_HANDLER_NOT_SPECIFIED, rc);
}

TEST(AgentPosixTest, CreateRejectsInvalidName) {
  ResultCode rc;
  auto agent = Agent::Create({"../escape", false},
                             std::make_unique<TestHandler>(), &rc);
  EXPECT_FALSE(agent);
  EXPECT_EQ(ResultCode::ERR_INVALID_CHANNEL_NAME, rc);
}

TEST(AgentPosixTest, CreateStopAndSocketCleanup) {
  SocketDirectory directory;
  TestHandler* handler;
  {
    RunningAgent agent(directory.SocketPath(), &handler);
    ASSERT_EQ(ResultCode::OK, agent.create_result());
    struct stat info;
    ASSERT_EQ(0, stat(directory.SocketPath().c_str(), &info));
    EXPECT_TRUE(S_ISSOCK(info.st_mode));
    EXPECT_EQ(0666u, info.st_mode & 0777);
  }
  struct stat info;
  EXPECT_NE(0, stat(directory.SocketPath().c_str(), &info));
}

TEST(AgentPosixTest, SecondAgentOnSameChannelFails) {
  SocketDirectory directory;
  TestHandler* handler;
  RunningAgent first(directory.SocketPath(), &handler);
  ASSERT_EQ(ResultCode::OK, first.create_result());

  ResultCode rc;
  auto second = Agent::Create({directory.SocketPath(), false},
                              std::make_unique<TestHandler>(), &rc);
  EXPECT_FALSE(second);
  EXPECT_EQ(ResultCode::ERR_AGENT_ALREADY_EXISTS, rc);

  // The live agent keeps its socket.
  struct stat info;
  EXPECT_EQ(0, stat(directory.SocketPath().c_str(), &info));
}

TEST(AgentPosixTest, StaleSocketIsReplaced) {
  SocketDirectory directory;
  {
    // Bind a socket and leave the file behind without listening.
    int fd = internal::CreateUnixSocket();
    ASSERT_GE(fd, 0);
    sockaddr_un address;
    memset(&address, 0, sizeof(address));
    address.sun_family = AF_UNIX;
    strncpy(address.sun_path, directory.SocketPath().c_str(),
            sizeof(address.sun_path) - 1);
    ASSERT_EQ(0, bind(fd, reinterpret_cast<const sockaddr*>(&address),
                      sizeof(address)));
    close(fd);
  }
  TestHandler* handler;
  RunningAgent agent(directory.SocketPath(), &handler);
  EXPECT_EQ(ResultCode::OK, agent.create_result());
}

TEST(AgentPosixTest, ClientWithoutAgentReturnsNull) {
  SocketDirectory directory;
  auto client = Client::Create({directory.SocketPath(), false});
  EXPECT_FALSE(client);
}

TEST(AgentPosixTest, RequestResponseAckCancel) {
  SocketDirectory directory;
  TestHandler* handler;
  RunningAgent agent(directory.SocketPath(), &handler);
  ASSERT_EQ(ResultCode::OK, agent.create_result());
  handler = agent.handler();

  auto client = Client::Create({directory.SocketPath(), false});
  ASSERT_TRUE(client);
  EXPECT_EQ(static_cast<unsigned long>(getpid()), client->GetAgentInfo().pid);
  EXPECT_FALSE(client->GetAgentInfo().binary_path.empty());

  ASSERT_TRUE(WaitFor([&] { return handler->connect_count.load() == 1; }));
  {
    std::lock_guard<std::mutex> lock(handler->mutex);
    EXPECT_EQ(static_cast<unsigned long>(getpid()), handler->last_info.pid);
    EXPECT_FALSE(handler->last_info.binary_path.empty());
  }

  ContentAnalysisResponse response;
  ASSERT_EQ(0, client->Send(MakeRequest("token-1", "hello"), &response));
  EXPECT_EQ("token-1", response.request_token());
  ASSERT_EQ(1, response.results_size());
  EXPECT_EQ("dlp", response.results(0).tag());
  EXPECT_EQ(ContentAnalysisResponse::Result::SUCCESS,
            response.results(0).status());
  EXPECT_EQ(0, response.results(0).triggered_rules_size());
  EXPECT_EQ(1, handler->request_count.load());

  ContentAnalysisAcknowledgement ack;
  ack.set_request_token("token-1");
  ack.set_status(ContentAnalysisAcknowledgement::SUCCESS);
  ack.set_final_action(ContentAnalysisAcknowledgement::ALLOW);
  ASSERT_EQ(0, client->Acknowledge(ack));
  ASSERT_TRUE(WaitFor([&] { return handler->ack_count.load() == 1; }));

  ContentAnalysisCancelRequests cancel;
  cancel.set_user_action_id("action-1");
  ASSERT_EQ(0, client->CancelRequests(cancel));
  ASSERT_TRUE(WaitFor([&] { return handler->cancel_count.load() == 1; }));

  client.reset();
  ASSERT_TRUE(WaitFor([&] { return handler->disconnect_count.load() == 1; }));
  EXPECT_EQ(0, handler->error_count.load());
}

TEST(AgentPosixTest, BlockVerdictReachesClient) {
  SocketDirectory directory;
  TestHandler* handler;
  RunningAgent agent(directory.SocketPath(), &handler, /*block=*/true);
  ASSERT_EQ(ResultCode::OK, agent.create_result());

  auto client = Client::Create({directory.SocketPath(), false});
  ASSERT_TRUE(client);
  ContentAnalysisResponse response;
  ASSERT_EQ(0, client->Send(MakeRequest("token-2", "block me"), &response));
  ASSERT_EQ(1, response.results_size());
  ASSERT_EQ(1, response.results(0).triggered_rules_size());
  EXPECT_EQ(ContentAnalysisResponse::Result::TriggeredRule::BLOCK,
            response.results(0).triggered_rules(0).action());
}

TEST(AgentPosixTest, LargeTextAndManyRequests) {
  SocketDirectory directory;
  TestHandler* handler;
  RunningAgent agent(directory.SocketPath(), &handler);
  ASSERT_EQ(ResultCode::OK, agent.create_result());
  handler = agent.handler();

  auto client = Client::Create({directory.SocketPath(), false});
  ASSERT_TRUE(client);

  std::string large(4 * 1024 * 1024, 'p');
  ContentAnalysisResponse response;
  ASSERT_EQ(0, client->Send(MakeRequest("big", large), &response));
  EXPECT_EQ("big", response.request_token());
  {
    std::lock_guard<std::mutex> lock(handler->mutex);
    EXPECT_EQ(large.size(), handler->last_text_size);
  }

  for (int i = 0; i < 50; ++i) {
    ASSERT_EQ(0, client->Send(MakeRequest("t" + std::to_string(i), "x"),
                              &response));
    EXPECT_EQ("t" + std::to_string(i), response.request_token());
  }
  EXPECT_EQ(51, handler->request_count.load());
}

TEST(AgentPosixTest, TwoBrowsersAtOnce) {
  SocketDirectory directory;
  TestHandler* handler;
  RunningAgent agent(directory.SocketPath(), &handler);
  ASSERT_EQ(ResultCode::OK, agent.create_result());
  handler = agent.handler();

  auto first = Client::Create({directory.SocketPath(), false});
  auto second = Client::Create({directory.SocketPath(), false});
  ASSERT_TRUE(first);
  ASSERT_TRUE(second);
  ASSERT_TRUE(WaitFor([&] { return handler->connect_count.load() == 2; }));

  ContentAnalysisResponse response;
  ASSERT_EQ(0, second->Send(MakeRequest("s", "x"), &response));
  EXPECT_EQ("s", response.request_token());
  ASSERT_EQ(0, first->Send(MakeRequest("f", "x"), &response));
  EXPECT_EQ("f", response.request_token());

  first.reset();
  second.reset();
  ASSERT_TRUE(WaitFor([&] { return handler->disconnect_count.load() == 2; }));
}

TEST(AgentPosixTest, MalformedFrameDisconnectsBrowser) {
  SocketDirectory directory;
  TestHandler* handler;
  RunningAgent agent(directory.SocketPath(), &handler);
  ASSERT_EQ(ResultCode::OK, agent.create_result());
  handler = agent.handler();

  int fd = internal::CreateUnixSocket();
  ASSERT_GE(fd, 0);
  sockaddr_un address;
  memset(&address, 0, sizeof(address));
  address.sun_family = AF_UNIX;
  strncpy(address.sun_path, directory.SocketPath().c_str(),
          sizeof(address.sun_path) - 1);
  ASSERT_EQ(0, connect(fd, reinterpret_cast<const sockaddr*>(&address),
                       sizeof(address)));
  ASSERT_TRUE(WaitFor([&] { return handler->connect_count.load() == 1; }));

  // A zero-length frame is a protocol error.
  char header[internal::kFrameHeaderSize] = {0, 0, 0, 0};
  ASSERT_EQ(0, internal::WriteAll(fd, header, sizeof(header)));
  ASSERT_TRUE(WaitFor([&] { return handler->disconnect_count.load() == 1; }));
  EXPECT_GE(handler->error_count.load(), 1);

  char byte;
  EXPECT_EQ(0, read(fd, &byte, 1));  // Agent closed the connection.
  close(fd);
}

TEST(AgentPosixTest, EventWithoutChannelCannotSend) {
  const BrowserInfo bi{12345, "/path/to/binary"};
  ContentAnalysisEventPosix event(bi, MakeRequest("t", "x"));
  EXPECT_EQ(ResultCode::OK, event.Init());
  EXPECT_EQ(ResultCode::ERR_BROKEN_PIPE, event.Send());
}

}  // namespace testing
}  // namespace sdk
}  // namespace content_analysis
