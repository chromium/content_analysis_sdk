# Copyright 2022 The Chromium Authors.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
cmake_minimum_required(VERSION 3.22)

project(chrome_enterprise_connector_local_analysis)

find_package(Protobuf CONFIG REQUIRED)

# The include directory contains the header files needed by the client code.
# The gen directory contains generated protobuf headers describing the request
# and response objects used to communicate with the agent.
include_directories(
  "${PROJECT_SOURCE_DIR}/../include"
  "${PROJECT_BINARY_DIR}/gen"
)

# The SDK contains platform specific code for each of the supported platforms.
# ${PLATFORM_CODE} holds the list of source files needed for the current
# platform being built.
if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
  set(PLATFORM_CODE
    ../src/client_win.cc
    ../src/client_win.h
  )
else()
  set(PLATFORM_CODE
    ../src/client_posix.cc
    ../src/client_posix.h
  )
endif()

# Builds the content analysis connector linker library.  This library is linked
# into the client in order to send content analysis requests to the agent.
add_library(cac
  ../include/content_analysis/sdk/analysis_client.h
  ../src/client_base.cc
  ../src/client_base.h
  ${PLATFORM_CODE}
  "${PROJECT_BINARY_DIR}/gen/content_analysis/sdk/analysis.pb.cc"
)
target_link_libraries(cac PUBLIC protobuf::libprotoc protobuf::libprotobuf protobuf::libprotobuf-lite)

# The demo executable.
add_executable(client client.cc)
target_link_libraries(client PRIVATE cac)
