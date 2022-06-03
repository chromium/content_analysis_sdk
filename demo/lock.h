// Copyright 2022 The Chromium Authors.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_ANALYSIS_DEMO_LOCK_H_
#define CONTENT_ANALYSIS_DEMO_LOCK_H_

#if defined(WIN32)
#include <windows.h>
#else
#include <pthread.h>
#endif

class ScopedLock;

// A lock used to serialize access to a shared request queue.
// See RequestQueue class for details.
class Lock {
 public:
  Lock() {
    Init();
  }
  ~Lock() {
    Term();
  }

 private:
  friend class ScopedLock;

#if defined(WIN32)
  void Init() {
    InitializeConditionVariable(&cv_);
    InitializeCriticalSection(&cs_);
  }
  void Term() {
    DeleteCriticalSection(&cs_);
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

  CRITICAL_SECTION cs_{};
  CONDITION_VARIABLE cv_{};
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
};

class ScopedLock {
 public:
  ScopedLock(Lock& lock) : lock_(lock) {}
  ~ScopedLock() = default;

  void Wait() {
    lock_.Wait();
  }
  void WakeOne() {
    lock_.WakeOne();
  }
  void WakeAll() {
    lock_.WakeAll();
  }

 private:
  Lock& lock_;
};

#endif  // CONTENT_ANALYSIS_DEMO_LOCK_H_