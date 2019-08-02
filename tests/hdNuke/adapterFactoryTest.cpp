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

#include <gmock/gmock.h>
#include <catch2/catch.hpp>

#include "mockObjects.h"

#include <DDImage/Scene.h>
#include <DDImage/Memory.h>
#include <DDImage/Allocators.h>
#include <DDImage/Iop.h>
#include <DDImage/GeoInfo.h>
#include <DDImage/Triangle.h>

#include <string>

PXR_NAMESPACE_USING_DIRECTIVE

using ::testing::Return;
using ::testing::Exactly;
using ::testing::Eq;

TEST_CASE("An HdNukeAdapterFactory") {
    HdNukeAdapterFactory factory = HdNukeAdapterFactory::Instance();
    TfToken mockAdapterToken{"MockGeoOp"};

    SECTION("Should support registering creator classes") {
        auto creator = std::make_shared<MockAdapterCreator>();
        factory.RegisterAdapterCreator(mockAdapterToken, creator);
        REQUIRE(factory.GetAdapterCreator(mockAdapterToken) == creator);
    }

    SECTION("Should return a previous creator when registering") {
        auto creator = std::make_shared<MockAdapterCreator>();
        auto creator2 = std::make_shared<MockAdapterCreator>();
        factory.RegisterAdapterCreator(mockAdapterToken, creator);
        auto previous = factory.RegisterAdapterCreator(mockAdapterToken, creator2);
        REQUIRE(previous == creator);
    }

    SECTION("Should support creating classes") {
        auto creator = std::make_shared<MockAdapterCreator>();
        factory.RegisterAdapterCreator(mockAdapterToken, creator);
        AdapterSharedState sharedState;

        EXPECT_CALL(*creator, Create(Eq(&sharedState)))
            .Times(Exactly(1))
            .WillOnce(Return(std::make_shared<MockAdapter>(&sharedState)));

        auto adapter = factory.Create(mockAdapterToken, &sharedState);
        REQUIRE(adapter != nullptr);
        REQUIRE(adapter->GetSharedState() == &sharedState);
    }

    factory.Clear();
}

