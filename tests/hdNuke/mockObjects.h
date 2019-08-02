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

#ifndef HDNUKE_MOCKOBJECTS_H
#define HDNUKE_MOCKOBJECTS_H

#include <gmock/gmock.h>

#include "../../src/hdNuke/adapter.h"
#include "../../src/hdNuke/adapterFactory.h"

#include <DDImage/GeoOp.h>
#include <DDImage/Triangle.h>

#include <pxr/pxr.h>
#include <pxr/imaging/hd/api.h>
#include <pxr/base/vt/value.h>
#include <pxr/base/tf/staticTokens.h>
#include <pxr/usd/sdf/path.h>

PXR_NAMESPACE_OPEN_SCOPE

struct AdapterSharedState;
class HdNukeAdapterManager;

class MockGeoOp : public DD::Image::GeoOp
{
public:
    MockGeoOp(Node* n) : GeoOp(n)
    {
    }

    const char* node_help() const override
    {
        return "MockGeoOp";
    }
    const char* Class() const override
    {
        return "MockGeoOp";
    }

    void geometry_engine(DD::Image::Scene& scene, DD::Image::GeometryList& out) override
    {
      out.add_object(0);
      auto triangle = new DD::Image::Triangle(0, 1, 2);
      out.add_primitive(0, triangle);
    }
};

class MockAdapter : public HdNukeAdapter
{
public:
    MockAdapter(AdapterSharedState* statePtr) : HdNukeAdapter(statePtr) { }
    ~MockAdapter() override { }

    MOCK_METHOD2(SetUp, bool(HdNukeAdapterManager*, const pxr::VtValue&));
    MOCK_METHOD2(Update, bool(HdNukeAdapterManager*, const pxr::VtValue&));
    MOCK_METHOD1(TearDown, void(HdNukeAdapterManager*));
    MOCK_CONST_METHOD0(GetPrimType, const TfToken&());
};

class MockAdapterCreator : public HdNukeAdapterFactory::AdapterCreator
{
public:
    MOCK_METHOD1(Create, HdNukeAdapterPtr(AdapterSharedState*));
};

PXR_NAMESPACE_CLOSE_SCOPE

#endif  // HDNUKE_MOCKOBJECTS_H
