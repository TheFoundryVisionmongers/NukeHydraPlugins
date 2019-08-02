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

#include "../../src/hdNuke/adapterFactory.h"
#include "../../src/hdNuke/adapterManager.h"
#include "../../src/hdNuke/sceneDelegate.h"

#include <DDImage/Scene.h>
#include <DDImage/Memory.h>
#include <DDImage/Allocators.h>
#include <DDImage/Iop.h>
#include <DDImage/GeoInfo.h>
#include <DDImage/Triangle.h>

#include <string>

PXR_NAMESPACE_USING_DIRECTIVE

using ::testing::Return;
using ::testing::ReturnRef;
using ::testing::Exactly;
using ::testing::Eq;
using ::testing::Invoke;
using ::testing::_;


TEST_CASE("A HdNukeAdapterManager") {
    DD::Image::Allocators::createDefaultAllocators();

    SECTION("Should be constructable") {
        HdNukeSceneDelegate sceneDelegate{nullptr};
        HdNukeAdapterManager manager{&sceneDelegate};
        REQUIRE(manager.GetSceneDelegate() == &sceneDelegate);
    }

    SECTION("Should manage the lifetime of adapters") {
        HdNukeSceneDelegate sceneDelegate{nullptr};
        HdNukeAdapterManager manager{&sceneDelegate};
        AdapterSharedState* sharedState = sceneDelegate.GetSharedState();
        auto mockAdapter = std::make_shared<MockAdapter>(sharedState);

        TfToken adapterType{"MockAdapter"};
        TfToken primType{"MockPrimType"};

        EXPECT_CALL(*mockAdapter, GetPrimType())
          .WillRepeatedly(ReturnRef(primType));

        auto creator = std::make_shared<MockAdapterCreator>();
        HdNukeAdapterFactory::Instance().RegisterAdapterCreator(adapterType, creator);

        SECTION("Requesting with an immediate construction") {
            EXPECT_CALL(*creator, Create(Eq(sharedState)))
                .Times(Exactly(1))
                .WillOnce(Return(mockAdapter));
            EXPECT_CALL(*mockAdapter, SetUp(Eq(&manager), Eq(VtValue{})))
                .Times(Exactly(1))
                .WillOnce(Return(true));

            HdNukeAdapterManager::AdapterPromisePtr promise = manager.Request(adapterType, SdfPath{"Mock/Primitive"}, {});

            SECTION("and return a promise") {
                REQUIRE(promise->adapter == mockAdapter);
                REQUIRE(promise->path == SdfPath{"/HdNuke/Mock/Primitive"});
            }

            SECTION("and add a mapping between its path and the adapter") {
                REQUIRE(manager.GetAdapter(promise->path) == mockAdapter);
            }

            SECTION("and add remember that the path is relative to the prim type") {
                auto primPaths = manager.GetPathsForPrimType(primType);
                REQUIRE(primPaths.find(promise->path) != primPaths.end());
            }

            SECTION("and mark the adapter as being requested") {
                auto requestedPaths = manager.GetRequestedAdapters();
                REQUIRE(requestedPaths.size() == 1);
                REQUIRE(requestedPaths.find(promise->path) != requestedPaths.end());
            }
        }

        SECTION("Requesting an adapter on the same path") {

            SECTION("If the prim type and nuke data are the same") {
              EXPECT_CALL(*creator, Create(Eq(sharedState)))
                  .Times(Exactly(1))
                  .WillOnce(Return(mockAdapter));
              EXPECT_CALL(*mockAdapter, SetUp(Eq(&manager), Eq(VtValue{})))
                  .Times(Exactly(1))
                  .WillOnce(Return(true));
              EXPECT_CALL(*mockAdapter, Update(Eq(&manager), Eq(VtValue{})))
                  .Times(Exactly(1))
                  .WillOnce(Return(true));

              HdNukeAdapterManager::AdapterPromisePtr promise = manager.Request(adapterType, SdfPath{"Mock/Primitive"}, {});
              HdNukeAdapterManager::AdapterPromisePtr promise2 = manager.Request(adapterType, SdfPath{"Mock/Primitive"}, {});

              REQUIRE(promise->path == promise2->path);
              REQUIRE(promise->adapter == promise2->adapter);
            }
        }

        SECTION("Requesting with a deferred construction") {
            EXPECT_CALL(*creator, Create(Eq(sharedState)))
                .Times(Exactly(1))
                .WillOnce(Return(mockAdapter));
            EXPECT_CALL(*mockAdapter, SetUp(Eq(&manager), Eq(VtValue{})))
                .Times(Exactly(1))
                .WillOnce(Return(false));
            HdNukeAdapterManager::AdapterPromisePtr promise = manager.Request(adapterType, SdfPath{"/Mock/Primitive"}, {});

            SECTION("Should mark the promise as unfulfilled") {
                REQUIRE(promise->adapter == nullptr);
            }

            SECTION("Should remember the promise was not fulfilled") {
                REQUIRE(manager.GetUnfulfilledPromise(promise->path) != nullptr);
            }

            SECTION("Should be able to fullfill promises") {
                EXPECT_CALL(*mockAdapter, SetUp(Eq(&manager), _))
                    .Times(Exactly(1))
                    .WillOnce(Return(true));
                manager.TryFulfillPromises();
                REQUIRE(promise->adapter != nullptr);
                REQUIRE(manager.GetUnfulfilledPromise(promise->path) == nullptr);
            }

            SECTION("Should be able to keep promises in an unfulfilled state") {
                EXPECT_CALL(*mockAdapter, SetUp(Eq(&manager), _))
                    .Times(Exactly(1))
                    .WillOnce(Return(false));
                manager.TryFulfillPromises();
                REQUIRE(promise->adapter == nullptr);
                REQUIRE(manager.GetUnfulfilledPromise(promise->path) != nullptr);
            }
        }

        SECTION("Removing an adapter") {
            EXPECT_CALL(*creator, Create(Eq(sharedState)))
                .Times(Exactly(1))
                .WillOnce(Return(mockAdapter));
            EXPECT_CALL(*mockAdapter, TearDown(Eq(&manager)))
                .Times(Exactly(1));

            SECTION("fully setup") {
                EXPECT_CALL(*mockAdapter, SetUp(Eq(&manager), Eq(VtValue{})))
                    .Times(Exactly(1))
                    .WillOnce(Return(true));
                HdNukeAdapterManager::AdapterPromisePtr promise = manager.Request(adapterType, SdfPath{"Mock/Primitive"}, {});
                manager.Remove(promise->path);
                REQUIRE(manager.GetAdapter(promise->path) == nullptr);
                REQUIRE(manager.GetPathsForPrimType(primType).size() == 0);
                REQUIRE(manager.GetUnfulfilledPromise(promise->path) == nullptr);
            }

            SECTION("in a unfulfilled state") {
                EXPECT_CALL(*mockAdapter, SetUp(Eq(&manager), Eq(VtValue{})))
                    .Times(Exactly(1))
                    .WillOnce(Return(false));
                HdNukeAdapterManager::AdapterPromisePtr promise = manager.Request(adapterType, SdfPath{"Mock/Primitive"}, {});
                manager.Remove(promise->path);
                REQUIRE(manager.GetAdapter(promise->path) == nullptr);
                REQUIRE(manager.GetPathsForPrimType(primType).size() == 0);
                REQUIRE(manager.GetUnfulfilledPromise(promise->path) == nullptr);
            }
        }

        SECTION("Requesting chained adapters") {
            auto mockAdapter2 = std::make_shared<MockAdapter>(sharedState);
            HdNukeAdapterManager::AdapterPromisePtr mockAdapter2Promise;
            EXPECT_CALL(*mockAdapter2, GetPrimType())
                .WillRepeatedly(ReturnRef(primType));

            EXPECT_CALL(*creator, Create(Eq(sharedState)))
              .WillOnce(Return(mockAdapter))
              .WillOnce(Return(mockAdapter2));
            EXPECT_CALL(*mockAdapter, SetUp(Eq(&manager), Eq(VtValue{})))
              .WillRepeatedly(Invoke([&](HdNukeAdapterManager* manager, const VtValue&) {
                  mockAdapter2Promise = manager->Request(adapterType, SdfPath{"Mock/Primitive2"}, {});
                  return mockAdapter2Promise->adapter != nullptr;
              }));

            SECTION("With an immediate construction") {
                EXPECT_CALL(*mockAdapter2, SetUp(Eq(&manager), Eq(VtValue{})))
                  .WillOnce(Return(true));

                HdNukeAdapterManager::AdapterPromisePtr promise = manager.Request(adapterType, SdfPath{"Mock/Primitive"}, {});

                REQUIRE(mockAdapter2Promise != nullptr);
                REQUIRE(mockAdapter2Promise->path == SdfPath("/HdNuke/Mock/Primitive2"));
                REQUIRE(mockAdapter2Promise->adapter == mockAdapter2);
            }

            SECTION("With a deferred construction") {
                EXPECT_CALL(*mockAdapter2, SetUp(Eq(&manager), Eq(VtValue{})))
                  .WillOnce(Return(false));

                HdNukeAdapterManager::AdapterPromisePtr promise = manager.Request(adapterType, SdfPath{"Mock/Primitive"}, {});

                REQUIRE(mockAdapter2Promise != nullptr);
                REQUIRE(mockAdapter2Promise->path == SdfPath("/HdNuke/Mock/Primitive2"));
                REQUIRE(mockAdapter2Promise->adapter == nullptr);

                REQUIRE(promise->adapter == nullptr);
                REQUIRE(manager.GetUnfulfilledPromise(promise->path) == promise);
                REQUIRE(manager.GetUnfulfilledPromise(mockAdapter2Promise->path) == mockAdapter2Promise);
            }

            SECTION("Should be able to fulfill promises of requestees") {
                EXPECT_CALL(*mockAdapter2, SetUp(Eq(&manager), _))
                  .WillOnce(Return(false))
                  .WillOnce(Return(true))
                  .WillRepeatedly(Invoke([&](HdNukeAdapterManager*, const VtValue&){
                      return mockAdapter2Promise->adapter != nullptr;
                  }));

                HdNukeAdapterManager::AdapterPromisePtr promise = manager.Request(adapterType, SdfPath{"Mock/Primitive"}, {});
                REQUIRE(promise->adapter == nullptr);
                HdNukeAdapterManager::AdapterPromisePtr promise2 = manager.Request(adapterType, SdfPath{"Mock/Primitive"}, {});
                REQUIRE(promise2->adapter != nullptr);
                REQUIRE(promise == promise2);

                REQUIRE(mockAdapter2Promise->adapter != nullptr);
                REQUIRE(promise->adapter != nullptr);
                REQUIRE(manager.GetUnfulfilledPromise(promise->path) == nullptr);
                REQUIRE(manager.GetUnfulfilledPromise(mockAdapter2Promise->path) == nullptr);
            }
        }

        SECTION("Requesting an unfulfilled adapter should try to fulfill it") {
            EXPECT_CALL(*creator, Create(Eq(sharedState)))
                .Times(Exactly(1))
                .WillOnce(Return(mockAdapter));
            EXPECT_CALL(*mockAdapter, SetUp(Eq(&manager), _))
                .Times(Exactly(2))
                .WillOnce(Return(false))
                .WillOnce(Return(true));

            HdNukeAdapterManager::AdapterPromisePtr promise = manager.Request(adapterType, SdfPath{"Mock/Primitive"}, {});
            REQUIRE(promise->adapter == nullptr);
            HdNukeAdapterManager::AdapterPromisePtr promise2 = manager.Request(adapterType, SdfPath{"Mock/Primitive"}, {});
            REQUIRE(promise2->adapter == mockAdapter);
            REQUIRE(promise == promise2);
        }

        SECTION("Make an unfulfilled adapter if it can't update") {
            EXPECT_CALL(*creator, Create(Eq(sharedState)))
                .Times(Exactly(1))
                .WillOnce(Return(mockAdapter));
            EXPECT_CALL(*mockAdapter, SetUp(Eq(&manager), _))
                .Times(Exactly(2))
                .WillOnce(Return(true))
                .WillOnce(Return(true));
            EXPECT_CALL(*mockAdapter, Update(Eq(&manager), _))
                .Times(Exactly(1))
                .WillOnce(Return(false));

            HdNukeAdapterManager::AdapterPromisePtr promise = manager.Request(adapterType, SdfPath{"Mock/Primitive"}, {});
            REQUIRE(promise->adapter == mockAdapter);
            HdNukeAdapterManager::AdapterPromisePtr promise2 = manager.Request(adapterType, SdfPath{"Mock/Primitive"}, {});
            REQUIRE(promise2->adapter == nullptr);
            REQUIRE(promise != promise2);
            HdNukeAdapterManager::AdapterPromisePtr promise3 = manager.Request(adapterType, SdfPath{"Mock/Primitive"}, {});
            REQUIRE(promise3->adapter == mockAdapter);
        }
    }

    SECTION("Should manage dependencies between adapters") {
        HdNukeSceneDelegate sceneDelegate{nullptr};
        HdNukeAdapterManager manager{&sceneDelegate};
        AdapterSharedState* sharedState = sceneDelegate.GetSharedState();
        auto mockAdapter = std::make_shared<MockAdapter>(sharedState);
        auto mockAdapter2 = std::make_shared<MockAdapter>(sharedState);

        TfToken adapterType{"MockAdapter"};
        TfToken primType{"MockPrimType"};

        auto creator = std::make_shared<MockAdapterCreator>();
        HdNukeAdapterFactory::Instance().RegisterAdapterCreator(adapterType, creator);
        HdNukeAdapterManager::AdapterPromisePtr promise2;

        EXPECT_CALL(*creator, Create(Eq(sharedState)))
            .Times(Exactly(2))
            .WillOnce(Return(mockAdapter))
            .WillOnce(Return(mockAdapter2));
        EXPECT_CALL(*mockAdapter, SetUp(Eq(&manager), Eq(VtValue{})))
            .Times(Exactly(1))
            .WillOnce(Invoke([&](HdNukeAdapterManager* manager, const VtValue& v) {
                promise2 = manager->Request(adapterType, SdfPath{"Mock/Primitive2"}, {});
                return true;
             }));
        EXPECT_CALL(*mockAdapter2, SetUp(Eq(&manager), Eq(VtValue{})))
            .Times(Exactly(1))
            .WillOnce(Return(true));
        EXPECT_CALL(*mockAdapter, GetPrimType())
            .WillRepeatedly(ReturnRef(primType));
        EXPECT_CALL(*mockAdapter2, GetPrimType())
            .WillRepeatedly(ReturnRef(primType));

        HdNukeAdapterManager::AdapterPromisePtr promise1 = manager.Request(adapterType, SdfPath{"Mock/Primitive1"}, {});

        SECTION("removing unused adapters") {
            EXPECT_CALL(*mockAdapter, TearDown(Eq(&manager)))
                .Times(Exactly(1));
            EXPECT_CALL(*mockAdapter2, TearDown(Eq(&manager)))
                .Times(Exactly(1));

            REQUIRE(manager.GetRequestedAdapters().size() == 2);
            manager.RemoveUnusedAdapters();
            REQUIRE(manager.GetAdapter(promise1->path) == mockAdapter);
            REQUIRE(manager.GetAdapter(promise2->path) == mockAdapter2);
            REQUIRE(manager.GetRequestedAdapters().size() == 0);

            manager.RemoveUnusedAdapters();
            REQUIRE(manager.GetAdapter(promise1->path) == nullptr);
            REQUIRE(manager.GetAdapter(promise2->path) == nullptr);
        }
    }

    SECTION("Should support requests for GeoInfos") {
        HdNukeSceneDelegate sceneDelegate{nullptr};
        HdNukeAdapterManager manager{&sceneDelegate};
        AdapterSharedState* sharedState = sceneDelegate.GetSharedState();
        auto mockAdapter = std::make_shared<MockAdapter>(sharedState);

        MockGeoOp geoOp{nullptr};
        DD::Image::Scene scene;
        geoOp.build_scene(scene);
        auto geo = scene.object(0);

        TfToken genericGeoInfo{"_GenericGeoInfo"};
        auto creator = std::make_shared<MockAdapterCreator>();
        HdNukeAdapterFactory::Instance().RegisterAdapterCreator(genericGeoInfo, creator);

        SECTION("With an immediate construction") {
            EXPECT_CALL(*creator, Create(Eq(sharedState)))
                .Times(Exactly(1))
                .WillOnce(Return(mockAdapter));
            EXPECT_CALL(*mockAdapter, SetUp(Eq(&manager), Eq(VtValue{&geo})))
                .Times(Exactly(1))
                .WillOnce(Return(true));
            EXPECT_CALL(*mockAdapter, GetPrimType())
                .WillRepeatedly(ReturnRef(HdPrimTypeTokens->mesh));
            HdNukeAdapterManager::AdapterPromisePtr promise = manager.Request(&geo);

            SECTION("and return a promise") {
                REQUIRE(promise->adapter == mockAdapter);
                REQUIRE(promise->path.GetString().find("/HdNuke/Geo/MockGeoOp/mesh_") == 0);
            }

            SECTION("and add a mapping between its path and the adapter") {
                REQUIRE(manager.GetAdapter(promise->path) == mockAdapter);
            }

            SECTION("and remember that the path is relative to a mesh") {
                auto primPaths = manager.GetPathsForPrimType(HdPrimTypeTokens->mesh);
                REQUIRE(primPaths.find(promise->path) != primPaths.end());
            }
        }

        SECTION("With a specific parent path") {
        }
    }

    SECTION("Should support requests for Iops") {
    }

    SECTION("Should support requests for LightOps") {
    }

    SECTION("Should allow registering adapters created externally") {
        HdNukeSceneDelegate sceneDelegate{nullptr};
        HdNukeAdapterManager manager{&sceneDelegate};
        AdapterSharedState* sharedState = sceneDelegate.GetSharedState();
        auto mockAdapter = std::make_shared<MockAdapter>(sharedState);
        TfToken primType{"MockPrimType"};

        auto promise = manager.AddAdapter(mockAdapter, primType, SdfPath{"Mock/Primitive"});

        SECTION("and return a fulfilled promise") {
            REQUIRE(promise->path == SdfPath{"/HdNuke/Mock/Primitive"});
            REQUIRE(promise->adapter == mockAdapter);
        }

        SECTION("and add a mapping between its path and the adapter") {
            REQUIRE(manager.GetAdapter(promise->path) == mockAdapter);
        }

        SECTION("and remember the path is relative to a MockPrimType") {
            auto primPaths = manager.GetPathsForPrimType(primType);
            REQUIRE(primPaths.find(promise->path) != primPaths.end());
        }
    }

    HdNukeAdapterFactory::Instance().Clear();
    DD::Image::Allocators::destroyDefaultAllocators();
}
