// Copyright 2005, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//     * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

// A sample program demonstrating using Google C++ testing framework.
//
// Author: wan@google.com (Zhanyong Wan)


// This sample shows how to write a more complex unit test for a class
// that has multiple member functions.
//
// Usually, it's a good idea to have one test for each method in your
// class.  You don't have to do that exactly, but it helps to keep
// your tests organized.  You may also throw in additional tests as
// needed.

// Hack to allow us to determine different bio flags based on kernel code. This
// mus now be compiled with kernel headers.
#include <linux/blk_types.h>

#include <vector>

#include "../../code/permuter/RandomPermuter.h"
#include "../../code/utils/utils.h"
#include "gtest/gtest.h"

namespace fs_testing {
namespace test {
using std::vector;

using fs_testing::utils::disk_write;
using fs_testing::permuter::RandomPermuter;

// Tests the default c'tor.
TEST(RandomPermuter, DefaultConstructor) {
  const RandomPermuter rp;
}

TEST(RandomPermuter, PermuteSingleEpoch) {
  unsigned int num_regular_writes = 9;
  vector<disk_write> test_epoch;

  for (unsigned int i = 0; i < num_regular_writes; ++i) {
    disk_write write;
    write.metadata.write_sector = 50 * i;
    write.metadata.size = 4096;
    write.metadata.bi_flags = REQ_WRITE;
    write.metadata.bi_rw = REQ_WRITE;

    // Make a sync operation.
    if (i % 3 == 0) {
      write.metadata.bi_flags |= REQ_SYNC;
      write.metadata.bi_rw |= REQ_SYNC;
    }
    test_epoch.push_back(write);
  }

  disk_write barrier;
  barrier.metadata.bi_flags = REQ_FUA;
  barrier.metadata.bi_rw = REQ_FUA;
  barrier.metadata.write_sector = 42;
  barrier.metadata.size = 8192;
  test_epoch.push_back(barrier);

  RandomPermuter rp;
  rp.set_data(&test_epoch);
  vector<disk_write> result;
  rp.permute(&result);
}

/*
const char kHelloString[] = "Hello, world!";

// Tests the c'tor that accepts a C string.
TEST(MyString, ConstructorFromCString) {
  const MyString s(kHelloString);
  EXPECT_EQ(0, strcmp(s.c_string(), kHelloString));
  EXPECT_EQ(sizeof(kHelloString)/sizeof(kHelloString[0]) - 1,
            s.Length());
}

// Tests the copy c'tor.
TEST(MyString, CopyConstructor) {
  const MyString s1(kHelloString);
  const MyString s2 = s1;
  EXPECT_EQ(0, strcmp(s2.c_string(), kHelloString));
}

// Tests the Set method.
TEST(MyString, Set) {
  MyString s;

  s.Set(kHelloString);
  EXPECT_EQ(0, strcmp(s.c_string(), kHelloString));

  // Set should work when the input pointer is the same as the one
  // already in the MyString object.
  s.Set(s.c_string());
  EXPECT_EQ(0, strcmp(s.c_string(), kHelloString));

  // Can we set the MyString to NULL?
  s.Set(NULL);
  EXPECT_STREQ(NULL, s.c_string());
}
*/

}  // namespace test
}  // namespace fs_testing
