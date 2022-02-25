# Copyright 2022 The Chromium Authors.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
cmake_minimum_required(VERSION 3.22)

project(chrome_enterprise_connector_local_analysis)

find_package(Protobuf CONFIG REQUIRED)

# The include directory contains the header files needed by the agent code.
# The gen directory contains generated protobuf headers describing the request
# and response objects used to communicate with Google Chrome.
include_directories(
  "${PROJECT_SOURCE_DIR}/../include"
  "${PROJECT_BINARY_DIR}/gen"
)

# The SDK contains platform specific code for each of the supported platforms.
# ${PLATFORM_CODE} holds the list of source files needed for the current
# platform being built.
if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
  set(PLATFORM_CODE
    ../src/agent_win.cc
    ../src/agent_win.h
    ../src/session_win.cc
    ../src/session_win.h
  )
else()
  set(PLATFORM_CODE
    ../src/agent_posix.cc
    ../src/agent_posix.h
    ../src/session_posix.cc
    ../src/session_posix.h
  )
endif()

# Builds the content analysis connector linker library.  This library is linked
# into the agent in order to listen for and process content analysis requests
# from Google Chrome.
add_library(cac
  ../include/content_analysis/sdk/analysis_agent.h
  ../src/agent_base.cc
  ../src/agent_base.h
  ../src/session_base.cc
  ../src/session_base.h
  ${PLATFORM_CODE}
  "${PROJECT_BINARY_DIR}/gen/content_analysis/sdk/analysis_agent.pb.cc"
)
target_link_libraries(cac PUBLIC protobuf::libprotoc protobuf::libprotobuf protobuf::libprotobuf-lite)

# The demo executable.
add_executable(agent agent.cc)
target_link_libraries(agent PRIVATE cac)
