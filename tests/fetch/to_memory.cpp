// Copyright 2016 The Emscripten Authors.  All rights reserved.
// Emscripten is available under two separate licenses, the MIT license and the
// University of Illinois/NCSA Open Source License.  Both these licenses can be
// found in the LICENSE file.

#include <string.h>
#include <stdio.h>
#include <math.h>
#include <assert.h>
#include <emscripten/fetch.h>

int result = 0;

// This test is run in two modes: if FILE_DOES_NOT_EXIST defined,
// then testing an XHR of a missing file.
// #define FILE_DOES_NOT_EXIST

int main() {
  emscripten_fetch_attr_t attr;
  emscripten_fetch_attr_init(&attr);
  strcpy(attr.requestMethod, "GET");
  attr.userData = (void*)0x12345678;
  attr.attributes = EMSCRIPTEN_FETCH_REPLACE | EMSCRIPTEN_FETCH_LOAD_TO_MEMORY;

  attr.onsuccess = [](emscripten_fetch_t *fetch) {
#if FILE_DOES_NOT_EXIST
    assert(false && "onsuccess handler called, but the file shouldn't exist"); // Shouldn't reach here if the file doesn't exist
#endif
    assert(fetch);
    printf("onsuccess: Finished downloading %llu bytes\n", fetch->numBytes);
    assert(fetch->url);
    assert(!strcmp(fetch->url, "gears.png"));
    assert(fetch->id != 0);
    assert((uintptr_t)fetch->userData == 0x12345678);
    assert(fetch->totalBytes == 6407);
    assert(fetch->numBytes == fetch->totalBytes);
    assert(fetch->data != 0);
    // Compute rudimentary checksum of data
    uint8_t checksum = 0;
    for(int i = 0; i < fetch->numBytes; ++i)
      checksum ^= fetch->data[i];
    printf("onsuccess: Data checksum: %02X\n", checksum);
    assert(checksum == 0x08);
    emscripten_fetch_close(fetch);

#ifdef REPORT_RESULT
    // Fetch API appears to sometimes call the handlers more than once, see https://github.com/emscripten-core/emscripten/pull/8191
    MAYBE_REPORT_RESULT(result);
#else
    exit(result);
#endif
  };

  attr.onprogress = [](emscripten_fetch_t *fetch) {
    assert(fetch);
    if (fetch->status != 200) return;
    printf("onprogress: dataOffset: %llu, numBytes: %llu, totalBytes: %llu\n", fetch->dataOffset, fetch->numBytes, fetch->totalBytes);
    if (fetch->totalBytes > 0) {
      printf("onprogress:  .. %.2f%% complete.\n", (fetch->dataOffset + fetch->numBytes) * 100.0 / fetch->totalBytes);
    } else {
      printf("onprogress:  .. %lld bytes complete.\n", fetch->dataOffset + fetch->numBytes);
    }
#ifdef FILE_DOES_NOT_EXIST
    assert(false && "onprogress handler called, but the file should not exist"); // We should not receive progress reports if the file doesn't exist.
#endif
    // We must receive a call to the onprogress handler with 100% completion.
    if (fetch->dataOffset + fetch->numBytes == fetch->totalBytes) result = 1;
    assert(fetch->dataOffset + fetch->numBytes <= fetch->totalBytes);
    assert(fetch->url);
    assert(!strcmp(fetch->url, "gears.png"));
    assert(fetch->id != 0);
    assert((uintptr_t)fetch->userData == 0x12345678);
  };

  attr.onerror = [](emscripten_fetch_t *fetch) {
    printf("onerror: Download failed!\n");
#ifndef FILE_DOES_NOT_EXIST
    assert(false && "onerror handler called, but the transfer should have succeeded!"); // The file exists, shouldn't reach here.
#endif
    assert(fetch);
    assert(fetch->id != 0);
    assert(!strcmp(fetch->url, "gears.png"));
    assert((uintptr_t)fetch->userData == 0x12345678);

#ifdef REPORT_RESULT
    // Fetch API appears to sometimes call the handlers more than once, see
    // https://github.com/emscripten-core/emscripten/pull/8191
    MAYBE_REPORT_RESULT(404);
#else
    exit(404);
#endif
  };

  emscripten_fetch_t *fetch = emscripten_fetch(&attr, "gears.png");
  assert(fetch != 0);
  // emscripten_fetch() must be able to operate without referencing to this structure after the call.
  memset(&attr, 0, sizeof(attr));
  return 0;
}
