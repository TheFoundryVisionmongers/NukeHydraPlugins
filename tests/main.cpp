// Copyright 2019-present The Foundry Visionmongers Ltd.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

#define CATCH_CONFIG_RUNNER
#include "catch2/catch.hpp"
#include "gmock/gmock.h"
#include <iostream>

class GMockCatchInterceptor
: public ::testing::EmptyTestEventListener {
public:
  void OnTestPartResult(::testing::TestPartResult const& gmock_assertion_result)
  {
    INFO(gmock_assertion_result.summary());
    REQUIRE_FALSE(gmock_assertion_result.failed());
  }
};


int main(int argc, char** argv)
{
  ::testing::InitGoogleTest(&argc, argv);

  ::testing::TestEventListeners& listeners = ::testing::UnitTest::GetInstance()->listeners();
  listeners.Append(new GMockCatchInterceptor);
  delete listeners.Release(listeners.default_result_printer());

  return Catch::Session().run(argc, argv);
}
