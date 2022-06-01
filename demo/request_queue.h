// Copyright 2022 The Chromium Authors.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_ANALYSIS_DEMO_REQUST_QUEUE_H_
#define CONTENT_ANALYSIS_DEMO_REQUST_QUEUE_H_

#include <memory>
#include <queue>

#include "content_analysis/sdk/analysis_agent.h"
#include "demo/lock.h"

// This class maintains a list of outstanding content analysis requests to
// process.  Each request is encapsulated in one ContentAnalysisEvent.
// Requests are handled in FIFO order.
class RequestQueue {
 public:
  using Event = content_analysis::sdk::ContentAnalysisEvent;

  RequestQueue() = default;
  virtual ~RequestQueue() = default;

  // Push a new content analysis event into the queue.
  void push(std::unique_ptr<Event> event) {
    ScopedLock sl(lock_);

    events_.push(std::move(event));

    // Wake before leaving to prevent unpredicatable scheduling.
    sl.WakeOne();
  }

  // Pop the next request from the queue, blocking if necessary until a event
  // is available.  Returns a nullptr if the queue will produce no more
  // events.
  std::unique_ptr<Event> pop() {
    ScopedLock sl(lock_);

    while (!abort_ && events_.size() == 0)
      sl.Wait();

    std::unique_ptr<Event> event;
    if (!abort_) {
      event = std::move(events_.front());
      events_.pop();
    }

    return event;
  }

  // Marks the queue as aborted.  pop() will now return nullptr.
  void abort() {
    ScopedLock sl(lock_);

    abort_ = true;

    // Wake before leaving to prevent unpredicatable scheduling.
    sl.WakeAll();
  }

 private:
  std::queue<std::unique_ptr<Event>> events_;
  Lock lock_;
  bool abort_ = false;
};

#endif  // CONTENT_ANALYSIS_DEMO_REQUST_QUEUE_H_