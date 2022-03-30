# Copyright 2022 The Chromium Authors.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
cmake_minimum_required(VERSION 3.22)

project(chrome_enterprise_connector_local_analysis)

# Set the path to the protoc protobuf compiler.
if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
  set(PROTOC ${PROJECT_BINARY_DIR}/vcpkg/installed/x64-windows/tools/protobuf/protoc.exe)
else()
  set(PROTOC ${PROJECT_BINARY_DIR}/vcpkg/installed/x64-linux/tools/protobuf/protoc)
endif()

# Loop through each proto and compile using protoc
file(MAKE_DIRECTORY ${PROJECT_BINARY_DIR}/gen)
# Calls the protoc compiler using the arguments specific to this project
# protobuf_generate_cpp is not flexible enough for our needs.
add_custom_command(
  OUTPUT ${PROJECT_BINARY_DIR}/gen/content_analysis/sdk/analysis.pb.cc
  COMMAND
    ${PROTOC}
    --cpp_out=${PROJECT_BINARY_DIR}/gen
    --proto_path=${PROJECT_SOURCE_DIR}/../proto
    ${PROJECT_SOURCE_DIR}/../proto/content_analysis/sdk/analysis.proto
  DEPENDS ../proto/content_analysis/sdk/analysis.proto
  WORKING_DIRECTORY ${PROJECT_BINARY_DIR}
)

# Define protos target. Compile this target exclusively by calling:
# `cmake --build <build_dir> --target protos`
add_custom_target(protos ALL DEPENDS ${PROJECT_BINARY_DIR}/gen/content_analysis/sdk/analysis.pb.cc)

# The include directory contains the header files needed by the demo code.
# The gen directory contains generated protobuf headers describing the request
# and response objects used to communicate with Google Chrome.
set(AGENT_INCLUDES
  "${PROJECT_SOURCE_DIR}/../agent/include"
  "${PROJECT_BINARY_DIR}/gen"
)
set(BROWSER_INCLUDES
  "${PROJECT_SOURCE_DIR}/../browser/include"
  "${PROJECT_BINARY_DIR}/gen"
)

# The SDK contains platform specific code for each of the supported platforms.
# ${PLATFORM_AGENT_CODE} holds the list of source files needed for the current
# platform being built.
if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
  set(PLATFORM_AGENT_CODE
    ../agent/src/agent_win.cc
    ../agent/src/agent_win.h
    ../agent/src/session_win.cc
    ../agent/src/session_win.h
  )
else()
  set(PLATFORM_AGENT_CODE
    ../agent/src/agent_posix.cc
    ../agent/src/agent_posix.h
    ../agent/src/session_posix.cc
    ../agent/src/session_posix.h
  )
endif()

# The SDK contains platform specific code for each of the supported platforms.
# ${PLATFORM_BROWSER_CODE} holds the list of source files needed for the current
# platform being built.
if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
  set(PLATFORM_BROWSER_CODE
    ../browser/src/client_win.cc
    ../browser/src/client_win.h
  )
else()
  set(PLATFORM_BROWSER_CODE
    ../browser/src/client_posix.cc
    ../browser/src/client_posix.h
  )
endif()

# Makes available the package definitions in vcpkg.
include("${PROJECT_BINARY_DIR}/vcpkg/scripts/buildsystems/vcpkg.cmake")
find_package(Protobuf CONFIG REQUIRED)

# Builds the content analysis connector agent linker library.  This library is linked
# into the agent in order to listen for and process content analysis requests
# from Google Chrome.
add_library(cac_agent
  ../agent/include/content_analysis/sdk/analysis_agent.h
  ../agent/src/agent_base.cc
  ../agent/src/agent_base.h
  ../agent/src/session_base.cc
  ../agent/src/session_base.h
  ${PLATFORM_AGENT_CODE}
  ${PROJECT_BINARY_DIR}/gen/content_analysis/sdk/analysis.pb.cc
)
target_link_libraries(cac_agent PUBLIC protobuf::libprotoc protobuf::libprotobuf protobuf::libprotobuf-lite)
target_include_directories(cac_agent PRIVATE ${AGENT_INCLUDES})
# Builds the content analysis connector browser linker library.  This library is linked
# into the client in order to send content analysis requests to the agent.
add_library(cac_browser
  ../browser/include/content_analysis/sdk/analysis_client.h
  ../browser/src/client_base.cc
  ../browser/src/client_base.h
  ${PLATFORM_BROWSER_CODE}
  ${PROJECT_BINARY_DIR}/gen/content_analysis/sdk/analysis.pb.cc
)
target_include_directories(cac_browser PRIVATE ${BROWSER_INCLUDES})
target_link_libraries(cac_browser PUBLIC protobuf::libprotoc protobuf::libprotobuf protobuf::libprotobuf-lite)

# The demo agent executable.
add_executable(agent agent.cc)
target_include_directories(agent PRIVATE ${AGENT_INCLUDES})
target_link_libraries(agent PRIVATE cac_agent)

# The demo client executable.
add_executable(browser client.cc)
target_include_directories(browser PRIVATE ${BROWSER_INCLUDES})
target_link_libraries(browser PRIVATE cac_browser)
