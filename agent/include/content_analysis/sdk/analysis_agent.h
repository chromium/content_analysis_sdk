// Copyright 2022 The Chromium Authors.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_ANALYSIS_AGENT_INCLUDE_CONTENT_ANALYSIS_SDK_ANALYSIS_AGENT_H_
#define CONTENT_ANALYSIS_AGENT_INCLUDE_CONTENT_ANALYSIS_SDK_ANALYSIS_AGENT_H_

#include <memory>
#include <string>

#include "content_analysis/sdk/analysis.pb.h"

// This is the main include file for code using Content Analysis Connector
// Agent SDK.  No other include is needed.
//
// An agent begins by creating an instance of Agent using the factory
// function Agent::Create().  This instance should live as long as the agent
// intends to receive content analysis requests from Google Chrome.
//
// Agent::Create() must be passed an object that implements the
// AgentEventHandler interface.  Methods on this interface will be called
// at the approriate time to handle specific events.  The events are:
//
// - A Google Chrome browser has started or stopped.  This events contains
//   information about the browser such as process id and executable path.
// - A request to analyze content.  The agent reads and analyses the
//   request in to determine a verdict: allow or block.  When the verdict is
//   known the response is sent back to Google Chrome.
// - An acknowledgement that Google Chrome has properly received the agent's
//   verdict.
//
// The agent is not required to serialize event handling.  That is, content
// analysis events can be analyze in the background and the response does
// not need to be sent before OnAnalysisRequested() returns.
//
// Google Chrome thottles the number of requests sent to the agent to 5
// current requests at a time but this is subject to change.

namespace content_analysis {
namespace sdk {

// Represents information about one instance of a Google Chrome browser
// process that is connected to the agent.
struct BrowserInfo {
  unsigned long pid;  // Process of Google Chrome browser process.
  std::string binary_path;  // The full path to the process's main binary.
};

// Represents one content analysis request as generated by a given user action
// in Google Chrome.
//
// The agent should retrieve information about the content analysis request
// using the GetRequest() method.  The agent should analyze the request and
// update the response, returned by GetResponse(), with a verdict (allow or
// block).  Once the verdict is set the response can be sent back to Google
// Chrome by calling Send().
//
// The default verdict is to allow the requested user action.  If the final
// verdict is to allow then the agent does not need to update the response and
// can simply call Send().
//
// If the final verdict should be to block, the agent should first update the
// response by calling SetEventVerdictToBlock() before calling Send().
//
// This class is not thread safe.  However, it may be passed to another thread
// as long as the agent properly serializses access to the event.
//
// See the demo directory for an example of how to use this class.
class ContentAnalysisEvent {
 public:
  virtual ~ContentAnalysisEvent() = default;

  // Prepares the event for graceful shutdown.  Upon return calls to all
  // other methods of this class will fail.  Returns 0 on success and -1
  // on failure.
  virtual int Close() = 0;

  // Retrives information about the browser that generated this content
  // analysis event.
  virtual const BrowserInfo& GetBrowserInfo() = 0;

  // Retrieves a read-only reference to the content analysis request received
  // from Google Chrome.
  virtual const ContentAnalysisRequest& GetRequest() const = 0;

  // Retrieves a writable reference to the content analysis response that will
  // be sent to Google Chrome as the verdict for the request of this event.
  // The agent may modify this response in place before calling Send().
  virtual ContentAnalysisResponse& GetResponse() = 0;

  // Send the verdict to Google Chrome.  Once this method is called further
  // changes to the response are ignored.  Returns 0 on success and -1
  // on failure.
  virtual int Send() = 0;

 protected:
  ContentAnalysisEvent() = default;
  ContentAnalysisEvent(const ContentAnalysisEvent& rhs) = delete;
  ContentAnalysisEvent(ContentAnalysisEvent&& rhs) = delete;
  ContentAnalysisEvent& operator=(const ContentAnalysisEvent& rhs) = delete;
  ContentAnalysisEvent& operator=(ContentAnalysisEvent&& rhs) = delete;
};

// Agents should implement this interface in order to handle events as needed.
//
// OnBrowserConnected() and OnBrowserDisonnected() notify the agent when
// instances of Google Chome start and stop.  The agent may perform any one-time
// actions as required for these events.  The default action is to do nothing
// for both events.  If the agent does not need perform any special actions
// these methods do not need to be overridden.
//
// OnAnalysisRequested() notifies the agent of a new content analysis request
// from Google Chrome.  The agent should perform the analysis and respond to
// the event.  It is not required for the agent complete the analysis and
// respond to before this callback returns.  The agent may pass the
// ContentAnalysisEvent to a background task and respond when ready.  This
// callback has no default action and agents must override it.
//
// OnResponseAcknowledged() notifies the agent that Google Chrome has received
// the content analysis response and how it has handled it.
class AgentEventHandler {
 public:
  AgentEventHandler() = default;
  virtual ~AgentEventHandler() = default;

  // Called when a new Google Chrome browser instance connects to the agent.
  // This is always called before the first OnAnalysisRequested() from that
  // browser.
  virtual void OnBrowserConnected(const BrowserInfo& info) {}

  // Called when a Google Chrome browser instance disconnects from the agent.
  // The agent will no longer receive new content analysis requests from this
  // browser.
  virtual void OnBrowserDisconnected(const BrowserInfo& info) {}

  // Called when a Google Chrome browser requests a content analysis.
  virtual void OnAnalysisRequested(
      std::unique_ptr<ContentAnalysisEvent> event) = 0;

  // Called when a Google Chrome browser acknowledges the content analysis
  // response from the agent.  The default action is to do nothing.
  // If the agent does not need perform any special actions this methods does
  // not need to be overridden.
  virtual void OnResponseAcknowledged(
      const ContentAnalysisAcknowledgement& ack) {}
};

// Represents an agent that can perform content analysis for the Google Chrome
// browser.  This class holds the server endpoint that Google Chrome connects
// to when content analysis is required.
//
// Agent instances should outlive all ContentAnalysisEvent instances created
// with it. Agent instances are not thread safe except for Stop() which can be
// called from any thread to shutdown the agent.  Outstanding
// ContentAnalysisEvents created from this agent may or may not still complete.
//
// See the demo directory for an example of how to use this class.
class Agent {
 public:
  // Configuration options where creating an agent.  `name` is used to create
  // a channel between the agent and Google Chrome.
  struct Config {
    // Used to create a channel between the agent and Google Chrome.  Both must
    // use the same name to properly rendezvous with each other.  The channel
    // is platform specific.
    std::string name;

    // Set to true if there is a different agent instance per OS user.  Defaults
    // to false.
    bool user_specific = false;
  };

  // Returns a new agent instance and calls Start().
  static std::unique_ptr<Agent> Create(
      Config config, std::unique_ptr<AgentEventHandler> handler);

  virtual ~Agent() = default;

  // Returns the configuration parameters used to create the agent.
  virtual const Config& GetConfig() const = 0;

  // Handles events triggered on this agent and calls the coresponding
  // callbacks in the AgentEventHandler.  This method is blocking and returns
  // when Stop() is called or if an error occurs.
  virtual void HandleEvents() = 0;

  // Prepares the agent for graceful shutdown.  Any function blocked on
  // HandleEvents() will return.  Returns 0 on success and -1 on failure.
  // It is safe to call this method from any thread.
  virtual int Stop() = 0;

 protected:
  Agent() = default;
  Agent(const Agent& rhs) = delete;
  Agent(Agent&& rhs) = delete;
  Agent& operator=(const Agent& rhs) = delete;
  Agent& operator=(Agent&& rhs) = delete;
};

// Update the tag or status of `response`.  This function assumes that the
// response contains only one Result.  If one already exists it is updated
// otherwise a new Result is created.
//
// The response contained within ContentAnalysisEvent has already been updated.
// This function is useful only when create a new instance of
// ContentAnalysisResponse.
//
// If `tag` is not empty it will replace the result's tag.
// If `status` is not STATUS_UNKNOWN it will will replace the result's status.
//
// Returns 0 on success and -1 on failure.
int UpdateResponse(ContentAnalysisResponse& response,
                   const std::string& tag,
                   ContentAnalysisResponse::Result::Status status);

// Sets the response verdict of an event to `action`.  This is a convenience
// function that is equivalent to the following:
//
//   auto result = event->GetResponse().mutable_results(0);
//   auto rule = result->mutable_triggered_rules(0);
//   rule->set_action(action);
//
// This function assumes the event's response has already been initialized
// using UpdateResponse().
//
// Returns 0 on success and -1 on failure.
int SetEventVerdictTo(
    ContentAnalysisEvent* event,
    ContentAnalysisResponse::Result::TriggeredRule::Action action);

// Sets the reponse verdict of an event to "block".  This is a convenience
// function that is equivalent to the following:
//
//   SetEventVerdictTo(event,
//                     ContentAnalysisResponse::Result::TriggeredRule::BLOCK);
//
// Returns 0 on success and -1 on failure.
int SetEventVerdictToBlock(ContentAnalysisEvent* event);

}  // namespace sdk
}  // namespace content_analysis

#endif  // CONTENT_ANALYSIS_AGENT_INCLUDE_CONTENT_ANALYSIS_SDK_ANALYSIS_AGENT_H_
