// Copyright 2022 The Chromium Authors.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if defined(WIN32)
#include <windows.h>
#else
#include <pthread.h>
#endif

#include <fstream>
#include <iostream>
#include <queue>
#include <string>

#include "content_analysis/sdk/analysis_agent.h"

using content_analysis::sdk::Agent;
using content_analysis::sdk::Session;
using content_analysis::sdk::ContentAnalysisRequest;
using content_analysis::sdk::ContentAnalysisResponse;

// This class maintains a list of outstanding content analysis requests to
// process.  Each request is encapsulated in one Session.  Requests are handled
// in FIFO order.
class RequestQueue {
 public:
  RequestQueue() {
    Init();
  }
  ~RequestQueue() {
    Term();
  }

  // Push a new content analysis session into the queue.
  void push(std::unique_ptr<Session> session) {
    Enter();
    sessions_.push(std::move(session));
    Leave();
    WakeOne();
  }

  // Pop the next request from the queue, blocking if necessary until a session
  // is available.  Returns a nullptr if the queue will produce no more
  // sessions.
  std::unique_ptr<Session> pop() {
    Enter();

    while (!abort_ && sessions_.size() == 0)
      Wait();

    std::unique_ptr<Session> session;
    if (!abort_) {
      session = std::move(sessions_.front());
      sessions_.pop();
    }

    Leave();
    return session;
  }

  // Marks the queue as aborted.  pop() will now return nullptr.
  void abort() {
    Enter();
    abort_ = true;
    Leave();
    WakeAll();
  }

 private:
#if defined(WIN32)
  void Init() {
    InitializeConditionVariable(&cv_);
    InitializeCriticalSection(&cs_);
  }
  void Term() {
  }
  void Enter() {
    EnterCriticalSection(&cs_);
  }
  void Leave() {
    LeaveCriticalSection(&cs_);
  }
  void Wait() {
    SleepConditionVariableCS(&cv_, &cs_, INFINITE);
  }
  void WakeOne() {
    WakeConditionVariable(&cv_);
  }
  void WakeAll() {
    WakeAllConditionVariable(&cv_);
  }

  CRITICAL_SECTION cs_;
  CONDITION_VARIABLE cv_;
#else
  void Init() {
    pthread_condattr_t cattr;
    pthread_condattr_init(&cattr);
    pthread_cond_init(&cv_, &cattr);

    pthread_mutexattr_t mattr;
    pthread_mutexattr_init(&mattr);
    pthread_mutex_init(&cs_, &mattr);
  }
  void Term() {
    pthread_mutex_destroy(&cs_);
    pthread_cond_destroy(&cv_);
  }
  void Enter() {
    pthread_mutex_lock(&cs_);
  }
  void Leave() {
    pthread_mutex_unlock(&cs_);
  }
  void Wait() {
    pthread_cond_wait(&cv_, &cs_);
  }
  void WakeOne() {
    pthread_cond_signal(&cv_);
  }
  void WakeAll() {
    pthread_cond_broadcast(&cv_);
  }

  pthread_mutex_t cs_;
  pthread_cond_t cv_;
#endif

  std::queue<std::unique_ptr<Session>> sessions_;
  bool abort_ = false;
};

void DumpRequest(const ContentAnalysisRequest& request) {
  std::string connector = "<Unknown>";
  if (request.has_analysis_connector()) {
    switch (request.analysis_connector())
    {
    case content_analysis::sdk::FILE_DOWNLOADED:
      connector = "download";
      break;
    case content_analysis::sdk::FILE_ATTACHED:
      connector = "attach";
      break;
    case content_analysis::sdk::BULK_DATA_ENTRY:
      connector = "bulk-data-entry";
      break;
    case content_analysis::sdk::PRINT:
      connector = "print";
      break;
    default:
      break;
    }
  }

  std::string url =
      request.has_request_data() && request.request_data().has_url()
      ? request.request_data().url() : "<No URL>";

  std::string filename =
      request.has_request_data() && request.request_data().has_filename()
      ? request.request_data().filename() : "<No filename>";

  std::string digest =
      request.has_request_data() && request.request_data().has_digest()
      ? request.request_data().digest() : "<No digest>";

  std::string file_path =
      request.has_content_data() && request.content_data().has_file_path()
      ? request.content_data().file_path() : "None, bulk text entry";

  std::cout << "Request: " << request.request_token() << std::endl;
  std::cout << "  Connector: " << connector << std::endl;
  std::cout << "  URL: " << url << std::endl;
  std::cout << "  Filename: " << filename << std::endl;
  std::cout << "  Digest: " << digest << std::endl;
  std::cout << "  Filepath: " << file_path << std::endl;
}

bool ReadContentFromFile(const std::string& file_path,
                         std::string* content) {
  std::ifstream file(file_path,
                     std::ios::in | std::ios::binary | std::ios::ate);
  if (!file.is_open())
    return false;

  // Get file size.  This example does not handle files larger than 1MB.
  // Make sure content string can hold the contents of the file.
  int size = file.tellg();
  if (size > 1024 * 1024)
    return false;

  content->resize(size + 1);

  // Read file into string.
  file.seekg(0, std::ios::beg);
  file.read(&(*content)[0], size);
  content->at(size) = 0;
  return true;
}

bool ShouldBlockRequest(const std::string& content) {
  // Determines if the request should be blocked.  For this simple example
  // the content is blocked if the string "block" is found.  Otherwise the
  // content is allowed.
  return content.find("block") != std::string::npos;
}

// Analyzes one request from Google Chrome and responds back to the browser
// with either an allow or block verdict.
void AnalyzeContent(std::unique_ptr<Session> session) {
  // A session represents one content analysis request and response triggered
  // by a user action in Google Chrome.  The agent determines whether the
  // user is allowed to perform the action by examining session->GetRequest().
  // The verdict, which can be "allow" or "block" is written into
  // session->GetResponse().

  DumpRequest(session->GetRequest());

  bool block = false;
  bool success = true;

  if (session->GetRequest().has_content_data() &&
      session->GetRequest().content_data().has_text_content()) {
    block = ShouldBlockRequest(
        session->GetRequest().content_data().text_content());
  } else if (session->GetRequest().has_content_data() &&
             session->GetRequest().content_data().has_file_path()) {
    std::string content;
    success =
        ReadContentFromFile(session->GetRequest().content_data().file_path(),
                            &content);
    if (success) {
      block = ShouldBlockRequest(content);
    }
  }

  if (!success) {
    content_analysis::sdk::UpdateResponse(session->GetResponse(), std::string(),
        ContentAnalysisResponse::Result::FAILURE);
    std::cout << "  Verdict: failed to reach verdict" << std::endl;
  } else if (block) {
    content_analysis::sdk::SetSessionVerdictToBlock(session.get());
    std::cout << "  Verdict: block" << std::endl;
  } else {
    std::cout << "  Verdict: allow" << std::endl;
  }

  std::cout << std::endl;

  // Send the response back to Google Chrome.
  if (session->Send() != 0) {
    std::cout << "[Demo] Error sending response" << std::endl;
  }
}

#if defined(WIN32)
unsigned _stdcall ProcessRequests(void* queue) {
#else
void* ProcessRequests(void* queue) {
#endif
  RequestQueue* request_queue = reinterpret_cast<RequestQueue*>(queue);

  while (true) {
    auto session = request_queue->pop();
    if (!session)
      break;

    AnalyzeContent(std::move(session));
  }

  return 0;
}

int main(int argc, char* argv[]) {
  // A list of outstanding content analysis requests.
  RequestQueue request_queue;

  // Start a background thread to process the queue.  This demo starts one
  // thread but any number would work.
#if defined(WIN32)
  unsigned tid;
  HANDLE thread = reinterpret_cast<HANDLE>(_beginthreadex(
      nullptr, 0, ProcessRequests, &request_queue, 0, &tid));
#else
  pthread_attr_t attr;
  pthread_t tid;
  pthread_attr_init(&attr);
  pthread_create(&tid, &attr, ProcessRequests, &request_queue);
  pthread_attr_destroy(&attr);
#endif

  // Each agent uses a unique URI to identify itself with Google Chrome.
  auto agent = Agent::Create("content_analysis_sdk");
  if (!agent) {
    std::cout << "[Demo] Error starting agent" << std::endl;
    return 1;
  };

  while (true) {
    // Creates a new content analysis session, blocking until one is sent to
    // the agent from Google Chrome.
    auto session = agent->GetNextSession();
    if (!session) {
      std::cout << "[Demo] Error starting session" << std::endl;
      return 1;
    }

    // Add the session to the request queue.  It will be processed by the
    // background thread in FIFO order.
    request_queue.push(std::move(session));
  }

  // Abort background process and wait for it to finish.
  request_queue.abort();
#if defined(WIN32)
  WaitForSingleObject(thread, INFINITE);
#else
  void* res;
  pthread_join(tid, &res);
#endif

  return 0;
};

