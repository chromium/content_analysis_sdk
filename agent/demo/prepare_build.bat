REM Copyright 2022 The Chromium Authors.
REM Use of this source code is governed by a BSD-style license that can be
REM found in the LICENSE file.
@echo off
setlocal

REM This script is meant to be run once to setup the example demo agent.
REM Run it with one command line argument: the path to a directory where the
REM demo agent will be built.  This should be a directory outside the SDK
REM directory tree.  This directory must not already exist.
REM
REM Once the build is prepared, the demo binary is build using the command
REM `cmake --build <build-dir>`, where <build-dir> is the same argument given
REM to this script.

set BUILD_DIR=%~f1
set DEMO_DIR=%~dp0
call :ABSPATH "%DEMO_DIR%..\.." ROOT_DIR
call :ABSPATH "%ROOT_DIR%\proto" PROTO_DIR

echo .
echo Root dir:   %ROOT_DIR%
echo Build dir:  %BUILD_DIR%
echo Demo dir:   %DEMO_DIR%
echo Proto dir:  %PROTO_DIR%
echo .

IF exist "%BUILD_DIR%" (
  echo.
  echo   ### Directory %1 must not exist.
  echo.
  EXIT /b
)

mkdir "%BUILD_DIR%"
cd /d "%BUILD_DIR%"

REM Install vcpkg and use it to install Google Protocol Buffers.
cmd/c git clone https://github.com/microsoft/vcpkg
cmd/c .\vcpkg\bootstrap-vcpkg.bat -disableMetrics
cmd/c .\vcpkg\vcpkg install protobuf:x64-windows

REM Generate proto files.
mkdir gen
.\vcpkg\installed\x64-windows\tools\protobuf\protoc.exe ^
  --cpp_out=gen ^
  --proto_path=%PROTO_DIR% ^
  %PROTO_DIR%\content_analysis\sdk\analysis.proto

REM Generate the build files.
set CMAKE_TOOLCHAIN_FILE=./vcpkg/scripts/buildsystems/vcpkg.cmake
cmake %DEMO_DIR%

echo.
echo.
echo To build, type: cmake --build "%BUILD_DIR%"
echo.

exit /b

REM Resolve relative path in %1 and set it into variable %2.
:ABSPATH
  set %2=%~f1
  exit /b

