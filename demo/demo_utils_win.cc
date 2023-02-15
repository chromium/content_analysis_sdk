// Copyright 2023 The Chromium Authors.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "demo/demo_utils.h"

#include <iostream>
#include <algorithm>
#include <windows.h>

int WriteHandleContentToFile(void* handle,
                             size_t size,
                             const std::string& path) {
  if (!handle)
    return -1;

  HANDLE file = CreateFileA(
    /* lpFileName */ path.c_str(),
    /* dwDesiredAccess */ GENERIC_WRITE,
    /* dwShareMode */ FILE_SHARE_READ,
    /* lpSecurityAttributes */nullptr,
    /* dwCreationDisposition */OPEN_ALWAYS,
    /* dwFlagsAndAttributes */FILE_ATTRIBUTE_NORMAL,
    /* hTemplateFile */nullptr
  );
  if (file == INVALID_HANDLE_VALUE)
    return GetLastError();

  // Write by chunks of 10 MB to handle very large data.
  constexpr size_t kChunkSize = 10 * 1024 * 1024;

  size_t total = 0;
  while (total != size) {
    DWORD high_offset = (total & 0xffffffff00000000) >> 32;
    DWORD low_offset = (total & 0x00000000ffffffff);
    size_t chunk_size = std::min<size_t>(kChunkSize, size - total);

    void* address = MapViewOfFile(handle,
                                  FILE_MAP_READ,
                                  high_offset,
                                  low_offset,
                                  chunk_size);

    DWORD bytes_written = 0;
    if (address &&
        WriteFile(file, address, chunk_size, &bytes_written, nullptr)) {
      total += bytes_written;
      bytes_written = 0;
    } else {
      CloseHandle(file);
      return GetLastError();
    }
  }

  if (!CloseHandle(file)) {
    return GetLastError();
  }
  return 0;
}
