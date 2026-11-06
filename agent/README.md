# Google Chrome Content Analysis Connector Agent SDK

This directory holds the Google Chrome Content Analysis Connector Agent SDK.
An Agent is an OS process running on the same computer as Google Chrome
that listens for and processes content analysis requests from the browser.
Supported OSes are Windows, Mac, and Linux.

## Google Protocol Buffers

This SDK depends on Google Protocol Buffers version 3.18 or later.  It may be
installed from Google's [download page](https://developers.google.com/protocol-buffers/docs/downloads#release-packages)
for your developement platform.  It may also be installed using a package
manager.

The included demo uses the Microsoft [vcpkg](https://github.com/microsoft/vcpkg)
package manager to install protobuf.  vcpkg is available on all supported
platforms.  See the demo for more details.

## Adding the SDK into an agent

Add the SDK to a content analysis agent as follows:

1. Clone the SDK from [Github](https://github.com/chromium/content_analysis_sdk).
This document assumes that the SDK is downloaded and extracted into the
directory $SDK_DIR.

2. Add the directory $SDK_DIR/include to the include path of the agent
code base.

3. Add all the source files in the directory $SDK_DIR/src to the agent build.
Note that files ending in _win.cc or _win.h are meant only for Windows, files
ending in _posix.cc or _posix.h are meant for Linux and Mac, and files ending
in _mac.cc or _mac.h are meant only for Mac.  `common/utils_posix.cc` is needed
on Linux and Mac.  See the top-level CMakeLists.txt for the exact lists.

4. Reference the SDK in agent code using:
```
#include "content_analysis/sdk/local_analysis.h"
```
## Channel between the browser and the agent

On Windows the channel is a message-mode named pipe.  On Mac and Linux it is a
Unix domain stream socket, and because stream sockets have no message
boundaries every message is prefixed with its length as a 32-bit little-endian
unsigned integer.  A message is one serialized `ChromeToAgent` (browser to
agent) or `AgentToChrome` (agent to browser).  Zero-length messages and
messages larger than 64 MiB are protocol errors and close the connection.

The socket path is derived from the channel name in `Agent::Config`:

| `user_specific` | Mac | Linux |
| --- | --- | --- |
| `false` (one agent for every OS user) | `/var/run/<name>.sock` | `/var/run/<name>.sock` |
| `true` (one agent per OS user) | `$(getconf DARWIN_USER_TEMP_DIR)<name>.sock` | `$XDG_RUNTIME_DIR/<name>.sock` |

A system agent must run as root to create its socket; `/var/run` is writable
only by root, which is what stops an unprivileged process from impersonating
the agent.  The socket is made connectable by every local user.  A per-user
agent's socket lives in a directory only that user can enter and is mode
0600.  There is no fallback to a world-writable directory: if the per-user
directory is unavailable the channel name is invalid.  A channel name that
starts with `/` is used verbatim as the socket path; this is intended for
tests and controlled deployments.

If an agent finds its socket file already present it connects to it first.
A live agent answering there is reported as `ERR_AGENT_ALREADY_EXISTS`; a
stale file left by a crashed agent is replaced.
